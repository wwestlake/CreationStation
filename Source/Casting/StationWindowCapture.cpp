#include "StationWindowCapture.h"
#include "BoundedFrameQueue.h"

#include <atomic>
#include <cstdio>
#include <thread>

#if JUCE_WINDOWS
#include <d3d11_4.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")
#endif

namespace cs::casting
{

#if JUCE_WINDOWS

struct StationWindowCapture::Impl
{
    using Item = winrt::Windows::Graphics::Capture::GraphicsCaptureItem;

    std::thread worker;
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { false };
    std::atomic<bool> targetClosed { false };
    HWND targetWindow = nullptr;
    BoundedFrameQueue<CapturedFrame> queue { 3 }; // ~100ms of backlog at 30fps; plenty to absorb a brief stall.

    ~Impl() { stop(); }

    void start(HWND window)
    {
        if (worker.joinable())
            return;
        targetWindow = window;
        stopRequested = false;
        targetClosed = false;
        worker = std::thread([this] { run(); });

        // Give the worker a window to report whether Windows Graphics Capture is even supported
        // before start() returns -- the caller needs that answer synchronously. Generous (up to
        // 5s): the very first D3D11CreateDevice/WGC call in a process can be genuinely slow
        // (cold driver/dll init), and a tight timeout here would misreport that as unavailable.
        for (int i = 0; i < 1000 && ! running.load() && ! initFailed.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (! running.load() && ! initFailed.load())
            fprintf(stderr, "StationWindowCapture: timed out waiting for capture to start (neither running nor failed after 5s)\n");
    }

    void stop()
    {
        stopRequested = true;
        if (worker.joinable())
            worker.join();
        running = false;
    }

    std::atomic<bool> initFailed { false };

    void run()
    {
        using namespace winrt::Windows::Graphics;
        using Clock = std::chrono::steady_clock;
        const auto sessionStart = Clock::now();

        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);

            if (! Capture::GraphicsCaptureSession::IsSupported())
            {
                initFailed = true;
                winrt::uninit_apartment();
                return;
            }

            // Note: the target window must be a real top-level application window (a taskbar
            // presence, a title bar) -- CreateForWindow returns E_INVALIDARG for a borderless
            // WS_EX_TOOLWINDOW popup. Station's own main window (a juce::DocumentWindow with a
            // native title bar) satisfies this; confirmed empirically during Work Order 3's
            // feasibility gate, which is also how that specific failure mode was found and
            // ruled out for production use.
            auto interopFactory = winrt::get_activation_factory<Item, IGraphicsCaptureItemInterop>();
            Item item { nullptr };
            winrt::check_hresult(interopFactory->CreateForWindow(targetWindow, winrt::guid_of<Item>(), winrt::put_abi(item)));

            winrt::com_ptr<ID3D11Device> device;
            winrt::com_ptr<ID3D11DeviceContext> context;
            winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put()));
            context.as<ID3D11Multithread>()->SetMultithreadProtected(TRUE);

            auto dxgiDevice = device.as<IDXGIDevice>();
            winrt::com_ptr<::IInspectable> inspectable;
            winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
            auto bridge = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

            auto size = item.Size();
            auto pool = Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                bridge, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
            auto session = pool.CreateCaptureSession(item);

            auto closedFlag = &targetClosed;
            item.Closed([closedFlag](auto&&, auto&&) { closedFlag->store(true); });

            session.StartCapture();
            running = true;

            while (! stopRequested.load() && ! targetClosed.load())
            {
                auto frame = pool.TryGetNextFrame();
                if (! frame)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(8));
                    continue;
                }

                auto content = frame.ContentSize();
                if (content.Width <= 0 || content.Height <= 0)
                {
                    frame.Close();
                    continue;
                }
                if (content.Width != size.Width || content.Height != size.Height)
                {
                    frame.Close();
                    size = content;
                    pool.Recreate(bridge, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
                    continue;
                }

                auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                winrt::com_ptr<ID3D11Texture2D> texture;
                winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));

                D3D11_TEXTURE2D_DESC desc {};
                texture->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.MiscFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                winrt::com_ptr<ID3D11Texture2D> staging;
                winrt::check_hresult(device->CreateTexture2D(&desc, nullptr, staging.put()));
                context->CopyResource(staging.get(), texture.get());
                context->Flush();

                D3D11_MAPPED_SUBRESOURCE mapped {};
                winrt::check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
                juce::Image image(juce::Image::ARGB, (int) size.Width, (int) size.Height, true, juce::SoftwareImageType {});
                {
                    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
                    auto* src = static_cast<const uint8_t*>(mapped.pData);
                    for (int y = 0; y < pixels.height; ++y)
                        memcpy(pixels.getLinePointer(y), src + (size_t) y * mapped.RowPitch, (size_t) pixels.lineStride);
                }
                context->Unmap(staging.get(), 0);
                frame.Close();

                const double elapsed = std::chrono::duration<double>(Clock::now() - sessionStart).count();
                queue.push({ std::move(image), elapsed });
            }

            session.Close();
            pool.Close();
            running = false;
            winrt::uninit_apartment();
        }
        catch (const winrt::hresult_error& e)
        {
            fprintf(stderr, "StationWindowCapture: hresult_error 0x%08x: %ls\n", (unsigned) e.code().value, e.message().c_str());
            initFailed = true;
            running = false;
        }
        catch (const std::exception& e)
        {
            fprintf(stderr, "StationWindowCapture: std::exception: %s\n", e.what());
            initFailed = true;
            running = false;
        }
    }
};

#else // ! JUCE_WINDOWS -- Windows Graphics Capture is a Windows-only API; this feature targets
      // the Windows PC the Behringer-interface/MIDI-controller workstation actually runs.

struct StationWindowCapture::Impl
{
    bool start(void*) { return false; }
    void stop() {}
    bool running() const { return false; }
};

#endif

StationWindowCapture::StationWindowCapture() : impl(std::make_unique<Impl>()) {}
StationWindowCapture::~StationWindowCapture() { stop(); }

bool StationWindowCapture::start(void* nativeWindowHandle)
{
#if JUCE_WINDOWS
    impl->start(static_cast<HWND>(nativeWindowHandle));
    return impl->running.load() && ! impl->initFailed.load();
#else
    juce::ignoreUnused(nativeWindowHandle);
    return false;
#endif
}

void StationWindowCapture::stop()
{
#if JUCE_WINDOWS
    impl->stop();
#endif
}

bool StationWindowCapture::isRunning() const noexcept
{
#if JUCE_WINDOWS
    return impl->running.load();
#else
    return false;
#endif
}

bool StationWindowCapture::tryPopFrame(CapturedFrame& out)
{
#if JUCE_WINDOWS
    return impl->queue.tryPop(out);
#else
    juce::ignoreUnused(out);
    return false;
#endif
}

size_t StationWindowCapture::getDroppedFrameCount() const
{
#if JUCE_WINDOWS
    return impl->queue.getDroppedCount();
#else
    return 0;
#endif
}

} // namespace cs::casting
