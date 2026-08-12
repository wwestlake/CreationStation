#include "VideoDecodeService.h"
#include "Bt709NV12Shader.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cstdint>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace cs
{
namespace
{
// Minimal, self-contained COM smart pointer - deliberately not JUCE's internal ComSmartPtr
// (not part of JUCE's public API surface) and not Microsoft::WRL::ComPtr (an extra SDK
// dependency this project doesn't otherwise take on) for a handful of interfaces used only here.
template <typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ComPtr(ComPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ~ComPtr() { reset(); }

    void reset()
    {
        if (ptr != nullptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }

    // Always resets first - matches the Windows convention that an out-param COM pointer must be
    // null before the call that's about to fill it in.
    T** address() noexcept { reset(); return &ptr; }
    T* get() const noexcept { return ptr; }
    T* operator->() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }

private:
    T* ptr = nullptr;
};

// COM apartment state is per-thread, not per-object - open()/decodeFrameAt()/
// decodeAudioToFloatPCM() can each be called from a different background thread than the one
// that constructed the VideoDecodeService, so each of those calls initializes COM on whichever
// thread is currently running it, scoped to just that call. Only uninitializes if this call was
// the one that actually initialized it (hr == S_OK, not S_FALSE for an already-initialized
// thread) - otherwise we'd tear down COM state some other owner on this thread still needs.
struct ScopedComInitializer
{
    ScopedComInitializer() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ScopedComInitializer()
    {
        if (SUCCEEDED(hr) && hr != S_FALSE)
            CoUninitialize();
    }
    HRESULT hr;
};

struct SharedD3D
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IMFDXGIDeviceManager> deviceManager;
    UINT resetToken = 0;
};

// Lazily created on first use and kept for the rest of the process's life. Recreating a D3D11
// device per VideoDecodeService instance would mean every open video track/clip pays for its own
// GPU device, and there's no safe point during normal operation to tear this down out from under
// a decode that might be in flight on another thread - process exit reclaims it regardless.
SharedD3D* getSharedD3D()
{
    static juce::CriticalSection initLock;
    static std::unique_ptr<SharedD3D> instance;
    static bool attempted = false;

    const juce::ScopedLock lock(initLock);
    if (attempted)
        return instance.get();

    attempted = true;
    auto candidate = std::make_unique<SharedD3D>();

    const UINT createFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL obtainedLevel {};

    auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
                                requestedLevels, (UINT) (sizeof(requestedLevels) / sizeof(requestedLevels[0])),
                                D3D11_SDK_VERSION, candidate->device.address(), &obtainedLevel,
                                candidate->context.address());
    if (FAILED(hr))
        return nullptr;

    hr = MFCreateDXGIDeviceManager(&candidate->resetToken, candidate->deviceManager.address());
    if (FAILED(hr))
        return nullptr;

    hr = candidate->deviceManager->ResetDevice(candidate->device.get(), candidate->resetToken);
    if (FAILED(hr))
        return nullptr;

    instance = std::move(candidate);
    return instance.get();
}

void ensureMediaFoundationStarted()
{
    static juce::CriticalSection lock;
    static bool started = false;
    const juce::ScopedLock sl(lock);
    if (! started)
    {
        MFStartup(MF_VERSION, MFSTARTUP_LITE);
        started = true;
    }
}

ComPtr<ID3DBlob> compileShader(const char* entryPoint, const char* target)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
   #if JUCE_DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
   #endif

    auto hr = D3DCompile(kBt709NV12ShaderSource, sizeof(kBt709NV12ShaderSource) - 1,
                         "Bt709NV12Shader", nullptr, nullptr, entryPoint, target,
                         flags, 0, blob.address(), errors.address());
    if (FAILED(hr))
    {
        if (errors)
            DBG("Bt709NV12Shader compile error (" << entryPoint << "): "
                << (const char*) errors->GetBufferPointer());
        blob.reset();
    }
    return blob;
}
}

struct VideoDecodeService::Impl
{
    ComPtr<IMFSourceReader> sourceReader;
    VideoStreamInfo info;
};

VideoDecodeService::VideoDecodeService() : impl(std::make_unique<Impl>())
{
    ScopedComInitializer comInit;
    ensureMediaFoundationStarted();
}

VideoDecodeService::~VideoDecodeService()
{
    close();
    // Deliberately no MFShutdown call - see getSharedD3D()'s comment; process-wide media
    // infrastructure here is intentionally never torn down mid-run.
}

void VideoDecodeService::close()
{
    if (impl)
    {
        impl->sourceReader.reset();
        impl->info = VideoStreamInfo {};
    }
}

bool VideoDecodeService::isOpen() const noexcept
{
    return impl && (bool) impl->sourceReader;
}

VideoStreamInfo VideoDecodeService::open(const juce::File& file)
{
    close();

    ScopedComInitializer comInit;
    auto* shared = getSharedD3D();
    if (shared == nullptr)
        return {};

    ComPtr<IMFAttributes> attributes;
    if (FAILED(MFCreateAttributes(attributes.address(), 3)))
        return {};

    attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, shared->deviceManager.get());
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    ComPtr<IMFSourceReader> reader;
    auto hr = MFCreateSourceReaderFromURL(file.getFullPathName().toWideCharPointer(), attributes.get(), reader.address());
    if (FAILED(hr))
        return {};

    // Ask for NV12 explicitly on the video stream. Without this the reader is free to pick its
    // own default output type (often a software-converted RGB32), which would silently defeat
    // the whole point of keeping decoded frames as GPU-resident NV12 textures.
    bool hasVideo = false;
    {
        ComPtr<IMFMediaType> videoType;
        if (SUCCEEDED(MFCreateMediaType(videoType.address())))
        {
            videoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            videoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            hasVideo = SUCCEEDED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, videoType.get()));
        }
    }

    if (! hasVideo)
        return {};

    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

    VideoStreamInfo streamInfo;
    streamInfo.valid = true;

    {
        ComPtr<IMFMediaType> actualVideoType;
        if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actualVideoType.address())))
        {
            UINT64 frameSize = 0;
            actualVideoType->GetUINT64(MF_MT_FRAME_SIZE, &frameSize);
            streamInfo.width = (int) (frameSize >> 32);
            streamInfo.height = (int) (frameSize & 0xffffffffu);

            UINT64 frameRateRatio = 0;
            actualVideoType->GetUINT64(MF_MT_FRAME_RATE, &frameRateRatio);
            auto numerator = (UINT32) (frameRateRatio >> 32);
            auto denominator = (UINT32) (frameRateRatio & 0xffffffffu);
            streamInfo.frameRate = denominator > 0 ? (double) numerator / (double) denominator : 0.0;
        }
    }

    {
        PROPVARIANT durationVar;
        PropVariantInit(&durationVar);
        if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durationVar)))
            streamInfo.durationSeconds = (double) durationVar.uhVal.QuadPart / 1.0e7; // 100-ns units
        PropVariantClear(&durationVar);
    }

    // Audio is optional - a video with no embedded audio (e.g. silent camera B-roll being
    // Foleyed from scratch) is a normal case, not a failure.
    {
        ComPtr<IMFMediaType> nativeAudioType;
        if (SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nativeAudioType.address())))
        {
            ComPtr<IMFMediaType> audioOutputType;
            if (SUCCEEDED(MFCreateMediaType(audioOutputType.address())))
            {
                audioOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                audioOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);

                if (SUCCEEDED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioOutputType.get())))
                {
                    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

                    ComPtr<IMFMediaType> actualAudioType;
                    if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, actualAudioType.address())))
                    {
                        UINT32 sampleRate = 0, channels = 0;
                        actualAudioType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
                        actualAudioType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
                        streamInfo.hasAudio = true;
                        streamInfo.audioSampleRate = (double) sampleRate;
                        streamInfo.audioNumChannels = (int) channels;
                    }
                }
                else
                {
                    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE);
                }
            }
        }
    }

    impl->sourceReader = std::move(reader);
    impl->info = streamInfo;
    return streamInfo;
}

juce::Image VideoDecodeService::decodeFrameAt(double sourceSeconds, int maxOutputWidth, int maxOutputHeight)
{
    if (! isOpen())
        return {};

    ScopedComInitializer comInit;
    auto* shared = getSharedD3D();
    if (shared == nullptr)
        return {};

    {
        PROPVARIANT seekVar;
        PropVariantInit(&seekVar);
        seekVar.vt = VT_I8;
        seekVar.hVal.QuadPart = (LONGLONG) juce::jmax(0.0, sourceSeconds * 1.0e7);
        impl->sourceReader->SetCurrentPosition(GUID_NULL, seekVar);
        PropVariantClear(&seekVar);
    }

    ComPtr<IMFSample> sample;
    {
        // A handful of retries: some containers hand back non-video "stream tick" events before
        // the first real video sample after a seek, which ReadSample surfaces as a call that
        // succeeds but returns a null sample.
        for (int attempt = 0; attempt < 8 && ! sample; ++attempt)
        {
            DWORD actualStreamIndex = 0, flags = 0;
            LONGLONG timestamp = 0;
            auto hr = impl->sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                                      &actualStreamIndex, &flags, &timestamp, sample.address());
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
                return {};
        }
    }

    if (! sample)
        return {};

    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->ConvertToContiguousBuffer(buffer.address())))
        return {};

    // Only a hardware-backed sample hands back a D3D11 texture through this interface - a sample
    // that doesn't means the decoder fell back to software (no hardware decoder for this codec),
    // which this service doesn't attempt to handle via a CPU path per its own design brief.
    ComPtr<IMFDXGIBuffer> dxgiBuffer;
    if (FAILED(buffer->QueryInterface(IID_PPV_ARGS(dxgiBuffer.address()))))
        return {};

    ComPtr<ID3D11Texture2D> sourceTexture;
    if (FAILED(dxgiBuffer->GetResource(IID_PPV_ARGS(sourceTexture.address()))))
        return {};

    UINT subresourceIndex = 0;
    dxgiBuffer->GetSubresourceIndex(&subresourceIndex);

    D3D11_TEXTURE2D_DESC sourceDesc {};
    sourceTexture->GetDesc(&sourceDesc);
    if (sourceDesc.Width == 0 || sourceDesc.Height == 0)
        return {};

    // Decoder-pool textures are arrays (one slice reused per in-flight frame), so the views need
    // TEXTURE2DARRAY pointed at this sample's specific slice, not a plain TEXTURE2D view.
    D3D11_SHADER_RESOURCE_VIEW_DESC lumaDesc {};
    lumaDesc.Format = DXGI_FORMAT_R8_UNORM;
    lumaDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    lumaDesc.Texture2DArray.MostDetailedMip = 0;
    lumaDesc.Texture2DArray.MipLevels = 1;
    lumaDesc.Texture2DArray.FirstArraySlice = subresourceIndex;
    lumaDesc.Texture2DArray.ArraySize = 1;

    ComPtr<ID3D11ShaderResourceView> lumaSrv;
    if (FAILED(shared->device->CreateShaderResourceView(sourceTexture.get(), &lumaDesc, lumaSrv.address())))
        return {};

    D3D11_SHADER_RESOURCE_VIEW_DESC chromaDesc = lumaDesc;
    chromaDesc.Format = DXGI_FORMAT_R8G8_UNORM;

    ComPtr<ID3D11ShaderResourceView> chromaSrv;
    if (FAILED(shared->device->CreateShaderResourceView(sourceTexture.get(), &chromaDesc, chromaSrv.address())))
        return {};

    const auto sourceWidth = (int) sourceDesc.Width;
    const auto sourceHeight = (int) sourceDesc.Height;
    const auto scale = juce::jmin((double) maxOutputWidth / (double) sourceWidth,
                                  (double) maxOutputHeight / (double) sourceHeight, 1.0);
    const auto outputWidth = juce::jmax(1, (int) std::lround(sourceWidth * scale));
    const auto outputHeight = juce::jmax(1, (int) std::lround(sourceHeight * scale));

    D3D11_TEXTURE2D_DESC rtDesc {};
    rtDesc.Width = (UINT) outputWidth;
    rtDesc.Height = (UINT) outputHeight;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> renderTargetTexture;
    if (FAILED(shared->device->CreateTexture2D(&rtDesc, nullptr, renderTargetTexture.address())))
        return {};

    ComPtr<ID3D11RenderTargetView> renderTargetView;
    if (FAILED(shared->device->CreateRenderTargetView(renderTargetTexture.get(), nullptr, renderTargetView.address())))
        return {};

    auto vertexShaderBlob = compileShader("VSMain", "vs_4_0");
    auto pixelShaderBlob = compileShader("PSMain", "ps_4_0");
    if (! vertexShaderBlob || ! pixelShaderBlob)
        return {};

    ComPtr<ID3D11VertexShader> vertexShader;
    if (FAILED(shared->device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(),
                                                  nullptr, vertexShader.address())))
        return {};

    ComPtr<ID3D11PixelShader> pixelShader;
    if (FAILED(shared->device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(),
                                                 nullptr, pixelShader.address())))
        return {};

    D3D11_SAMPLER_DESC samplerDesc {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ComPtr<ID3D11SamplerState> samplerState;
    if (FAILED(shared->device->CreateSamplerState(&samplerDesc, samplerState.address())))
        return {};

    auto* context = shared->context.get();

    D3D11_VIEWPORT viewport {};
    viewport.Width = (float) outputWidth;
    viewport.Height = (float) outputHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    ID3D11RenderTargetView* rtvs[] = { renderTargetView.get() };
    context->OMSetRenderTargets(1, rtvs, nullptr);
    context->RSSetViewports(1, &viewport);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(nullptr);
    context->VSSetShader(vertexShader.get(), nullptr, 0);
    context->PSSetShader(pixelShader.get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { lumaSrv.get(), chromaSrv.get() };
    context->PSSetShaderResources(0, 2, srvs);
    ID3D11SamplerState* samplers[] = { samplerState.get() };
    context->PSSetSamplers(0, 1, samplers);

    context->Draw(3, 0);

    // Unbind before the decoder-pool slice this sample points at potentially gets reused for the
    // next decoded frame - leaving stale SRVs bound to it would be a dangling-view hazard.
    ID3D11ShaderResourceView* nullSrvs[] = { nullptr, nullptr };
    context->PSSetShaderResources(0, 2, nullSrvs);

    D3D11_TEXTURE2D_DESC stagingDesc = rtDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    if (FAILED(shared->device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.address())))
        return {};

    context->CopyResource(stagingTexture.get(), renderTargetTexture.get());

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return {};

    juce::Image image(juce::Image::ARGB, outputWidth, outputHeight, false);
    {
        juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::writeOnly);
        auto* srcBase = static_cast<const uint8_t*>(mapped.pData);

        for (int y = 0; y < outputHeight; ++y)
        {
            auto* srcRow = srcBase + (size_t) y * mapped.RowPitch;

            for (int x = 0; x < outputWidth; ++x)
            {
                auto r = srcRow[x * 4 + 0];
                auto g = srcRow[x * 4 + 1];
                auto b = srcRow[x * 4 + 2];
                auto a = srcRow[x * 4 + 3];
                bitmapData.setPixelColour(x, y, juce::Colour(r, g, b, a));
            }
        }
    }
    context->Unmap(stagingTexture.get(), 0);

    return image;
}

bool VideoDecodeService::decodeAudioToFloatPCM(juce::AudioBuffer<float>& destination)
{
    if (! isOpen() || ! impl->info.hasAudio)
        return false;

    ScopedComInitializer comInit;

    {
        PROPVARIANT seekVar;
        PropVariantInit(&seekVar);
        seekVar.vt = VT_I8;
        seekVar.hVal.QuadPart = 0;
        impl->sourceReader->SetCurrentPosition(GUID_NULL, seekVar);
        PropVariantClear(&seekVar);
    }

    std::vector<float> interleaved;
    const auto numChannels = juce::jmax(1, impl->info.audioNumChannels);

    for (;;)
    {
        ComPtr<IMFSample> sample;
        DWORD actualStreamIndex = 0, flags = 0;
        LONGLONG timestamp = 0;
        auto hr = impl->sourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                                  &actualStreamIndex, &flags, &timestamp, sample.address());
        if (FAILED(hr))
            return false;

        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
            break;

        if (! sample)
            continue;

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(buffer.address())))
            continue;

        BYTE* data = nullptr;
        DWORD dataLength = 0;
        if (FAILED(buffer->Lock(&data, nullptr, &dataLength)))
            continue;

        auto* samples = reinterpret_cast<const float*>(data);
        auto sampleCount = dataLength / sizeof(float);
        interleaved.insert(interleaved.end(), samples, samples + sampleCount);

        buffer->Unlock();
    }

    if (interleaved.empty())
        return false;

    const auto numFrames = (int) (interleaved.size() / (size_t) numChannels);
    destination.setSize(numChannels, numFrames);

    for (int frame = 0; frame < numFrames; ++frame)
        for (int channel = 0; channel < numChannels; ++channel)
            destination.setSample(channel, frame, interleaved[(size_t) frame * (size_t) numChannels + (size_t) channel]);

    return true;
}
}
