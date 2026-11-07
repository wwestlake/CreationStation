#include "WinRTCastingBackend.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <shobjidl_core.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Casting.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>

#pragma comment(lib, "windowsapp.lib")
#endif

namespace cs::casting
{

#if JUCE_WINDOWS

namespace Casting = winrt::Windows::Media::Casting;
namespace Playback = winrt::Windows::Media::Playback;
namespace MediaCore = winrt::Windows::Media::Core;
namespace Foundation = winrt::Windows::Foundation;

struct WinRTCastingBackend::Impl
{
    HWND ownerHwnd = nullptr;
    bool apartmentReady = false;
    bool pickerAvailable = false;

    Casting::CastingDevicePicker picker { nullptr };
    Casting::CastingDevice selectedDevice { nullptr };
    Casting::CastingConnection connection { nullptr };
    Playback::MediaPlayer player { nullptr };

    std::function<void(CastState)> onStateChanged;
    std::function<void(const CastError&)> onError;
    std::function<void(const CastDevice&)> onDeviceSelected;

    explicit Impl(HWND hwnd) : ownerHwnd(hwnd)
    {
        try
        {
            // Deliberately never paired with winrt::uninit_apartment() in the destructor: JUCE's
            // own Windows backend already manages COM on this (the UI) thread for its own needs
            // (drag-and-drop, common dialogs), and this call either joins that existing
            // apartment (S_FALSE, the expected case) or -- if some future JUCE change used a
            // genuinely incompatible threading model -- throws here, which we catch and treat
            // as "casting unavailable" rather than disturbing whatever COM state the rest of the
            // app already depends on.
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            apartmentReady = true;

            picker = Casting::CastingDevicePicker();
            auto initWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initWithWindow->Initialize(ownerHwnd));

            auto filter = picker.Filter();
            filter.SupportsVideo(true);

            picker.CastingDeviceSelected([this](Casting::CastingDevicePicker const&, Casting::CastingDeviceSelectedEventArgs const& args)
            {
                handleDeviceSelected(args.SelectedCastingDevice());
            });
            picker.CastingDevicePickerDismissed([this](Casting::CastingDevicePicker const&, auto const&)
            {
                handlePickerDismissed();
            });

            pickerAvailable = true;
        }
        catch (const winrt::hresult_error&)
        {
            pickerAvailable = false;
        }
    }

    ~Impl()
    {
        teardownConnection();
    }

    void beginDiscovery(juce::Rectangle<int> anchorScreenBounds)
    {
        if (! pickerAvailable)
        {
            report(CastState::failed, { CastErrorCode::castingApiUnavailable, "Windows casting isn't available on this PC." });
            return;
        }

        report(CastState::searching, {});
        Foundation::Rect anchor {
            (float) anchorScreenBounds.getX(), (float) anchorScreenBounds.getY(),
            (float) anchorScreenBounds.getWidth(), (float) anchorScreenBounds.getHeight()
        };
        try
        {
            picker.Show(anchor);
        }
        catch (const winrt::hresult_error& e)
        {
            report(CastState::failed, { CastErrorCode::castingApiUnavailable, winrt::to_string(e.message()) });
        }
    }

    void cancelDiscovery()
    {
        if (pickerAvailable)
        {
            try { picker.Hide(); } catch (const winrt::hresult_error&) {}
        }
    }

    void startCasting(const juce::String& streamUrl)
    {
        if (! selectedDevice)
        {
            report(CastState::failed, { CastErrorCode::internalError, "No device was chosen before casting began." });
            return;
        }

        try
        {
            connection = selectedDevice.CreateCastingConnection();
            connection.StateChanged([this](Casting::CastingConnection const& conn, Foundation::IInspectable const&)
            {
                handleStateChanged(conn.State());
            });
            connection.ErrorOccurred([this](Casting::CastingConnection const&, Casting::CastingConnectionErrorOccurredEventArgs const& args)
            {
                juce::ignoreUnused(args);
                report(CastState::failed, { CastErrorCode::connectionRejected, "The device reported a casting error." });
            });

            player = Playback::MediaPlayer();
            Foundation::Uri uri { winrt::to_hstring(streamUrl.toStdString()) };
            player.Source(MediaCore::MediaSource::CreateFromUri(uri));
            auto source = player.GetAsCastingSource();

            report(CastState::connecting, {});

            // RequestStartCastingAsync's result is a CastingConnectionErrorStatus (not just a
            // pass/fail IAsyncAction) -- confirmed against the installed Windows SDK headers by
            // the feasibility-gate build itself (the compiler rejected an IAsyncAction-shaped
            // handler here, which is exactly the mandatory-gate's "verify the real API shape"
            // check doing its job).
            auto asyncOp = connection.RequestStartCastingAsync(source);
            asyncOp.Completed([this](Foundation::IAsyncOperation<Casting::CastingConnectionErrorStatus> const& operation, Foundation::AsyncStatus status)
            {
                if (status == Foundation::AsyncStatus::Error)
                {
                    report(CastState::failed, { CastErrorCode::connectionRejected, "The device declined the casting request." });
                    return;
                }
                if (status != Foundation::AsyncStatus::Completed)
                    return;

                const auto errorStatus = operation.GetResults();
                if (errorStatus != Casting::CastingConnectionErrorStatus::Succeeded)
                {
                    report(CastState::failed, { CastErrorCode::connectionRejected,
                        "Casting request result: " + juce::String((int) errorStatus) });
                }
                // On success, connection.StateChanged (already wired above) drives the
                // connecting -> connected transition; nothing further to do here.
            });
        }
        catch (const winrt::hresult_error& e)
        {
            report(CastState::failed, { CastErrorCode::connectionRejected, winrt::to_string(e.message()) });
            teardownConnection();
        }
    }

    void stopCasting()
    {
        if (! connection && ! player)
            return;
        report(CastState::disconnecting, {});
        teardownConnection();
        report(CastState::disconnected, {});
    }

    void handleDeviceSelected(const Casting::CastingDevice& device)
    {
        selectedDevice = device;
        if (onDeviceSelected)
        {
            juce::String name;
            try { name = juce::String(winrt::to_string(device.FriendlyName())); }
            catch (...) { name = "Cast device"; }
            onDeviceSelected({ juce::String(winrt::to_string(device.Id())), name });
        }
    }

    void handlePickerDismissed()
    {
        if (! selectedDevice)
            report(CastState::idle, {});
    }

    void handleStateChanged(Casting::CastingConnectionState state)
    {
        switch (state)
        {
            case Casting::CastingConnectionState::Connecting:    report(CastState::connecting, {}); break;
            case Casting::CastingConnectionState::Connected:
            case Casting::CastingConnectionState::Rendering:     report(CastState::connected, {}); break;
            case Casting::CastingConnectionState::Disconnecting: report(CastState::disconnecting, {}); break;
            case Casting::CastingConnectionState::Disconnected:  report(CastState::disconnected, {}); teardownConnection(); break;
            default: break;
        }
    }

    void teardownConnection()
    {
        if (player)
        {
            try { player.Pause(); } catch (...) {}
            player = nullptr;
        }
        connection = nullptr;
        selectedDevice = nullptr;
    }

    void report(CastState state, const CastError& error)
    {
        if (error.isSet() && onError)
            onError(error);
        if (onStateChanged)
            onStateChanged(state);
    }
};

#else // ! JUCE_WINDOWS -- Windows.Media.Casting is a Windows-only API.

struct WinRTCastingBackend::Impl
{
    explicit Impl(void*) {}
    bool pickerAvailable = false;
    std::function<void(CastState)> onStateChanged;
    std::function<void(const CastError&)> onError;
    std::function<void(const CastDevice&)> onDeviceSelected;

    void beginDiscovery(juce::Rectangle<int>)
    {
        if (onError) onError({ CastErrorCode::castingApiUnavailable, "Casting to TV is only available on Windows." });
        if (onStateChanged) onStateChanged(CastState::failed);
    }
    void cancelDiscovery() {}
    void startCasting(const juce::String&) {}
    void stopCasting() {}
};

#endif

WinRTCastingBackend::WinRTCastingBackend(juce::Component& ownerWindow)
{
#if JUCE_WINDOWS
    auto* peer = ownerWindow.getPeer();
    auto hwnd = peer != nullptr ? (HWND) peer->getNativeHandle() : nullptr;
    impl = std::make_unique<Impl>(hwnd);
#else
    juce::ignoreUnused(ownerWindow);
    impl = std::make_unique<Impl>(nullptr);
#endif
    impl->onStateChanged = [this](CastState s) { if (onStateChanged) onStateChanged(s); };
    impl->onError = [this](const CastError& e) { if (onError) onError(e); };
    impl->onDeviceSelected = [this](const CastDevice& d) { if (onDeviceSelected) onDeviceSelected(d); };
}

WinRTCastingBackend::~WinRTCastingBackend() = default;

void WinRTCastingBackend::beginDiscovery(juce::Rectangle<int> anchorScreenBounds) { impl->beginDiscovery(anchorScreenBounds); }
void WinRTCastingBackend::cancelDiscovery() { impl->cancelDiscovery(); }
void WinRTCastingBackend::startCasting(const juce::String& streamUrl) { impl->startCasting(streamUrl); }
void WinRTCastingBackend::stopCasting() { impl->stopCasting(); }
bool WinRTCastingBackend::isAvailable() const noexcept { return impl->pickerAvailable; }

} // namespace cs::casting
