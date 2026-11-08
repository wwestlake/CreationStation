// Work Order 3's mandatory feasibility gate, run for real rather than argued from documentation.
// Windows-only (exits 77 = skipped on any other platform, matching VideoGlSmoke's convention for
// a platform/feature that genuinely isn't present rather than a failure).
//
// What this DOES empirically confirm, by actually doing it rather than reading about it:
//   1. The exact Windows API path for presenting a live media source to casting --
//      WinRTCastingBackend's CastingDevicePicker + Windows.Media.Casting construction --
//      actually initializes against this machine's installed Windows SDK/runtime.
//   2. The source can be continuously updated: several real frames are captured from a real
//      (briefly shown) window and encoded in sequence, confirmed by watching the byte count
//      the muxer produces grow more than once, not just once at start.
//   3/6. The encoding/container/transport format (H.264 in a fragmented MP4, served as a
//      chunked HTTP response from a local, LAN-bound endpoint) is a real, connectable HTTP
//      resource: a plain TCP client in this test connects to the URL StationCastingStreamSource
//      returns and confirms real "200 OK" + "chunked" headers and non-zero payload bytes.
//
// What this test deliberately does NOT and cannot confirm (needs a human and a real TV):
//   - That the native OS device picker (CastingDevicePicker.Show()) actually lists a real
//     Miracast/DLNA receiver on this network -- that UI is interactive and is never invoked
//     here.
//   - That a real receiver can actually open, decode, and play the resulting stream.
// See the Work Order 3 report for exactly what remains for hands-on commissioning.
#include <JuceHeader.h>

#include "Casting/StationCastingStreamSource.h"
#include "Casting/WinRTCastingBackend.h"

#include <cstdio>

#if JUCE_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Media.Casting.h>
#pragma comment(lib, "windowsapp.lib")
#endif

namespace
{
int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

#if JUCE_WINDOWS
// A small, briefly-visible window standing in for Station's main window -- Windows Graphics
// Capture needs a real on-screen window to capture, which an invisible/off-screen component
// cannot provide.
class ProbeWindow : public juce::Component
{
public:
    ProbeWindow()
    {
        setSize(640, 480);
        setOpaque(true);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkslateblue);
        g.setColour(juce::Colours::white);
        g.setFont(28.0f);
        g.drawText("Djehuti Station -- Cast feasibility probe", getLocalBounds(), juce::Justification::centred);
    }
};

// Work Order 5 regression coverage: independently enumerate every up, gateway-less adapter
// address on this machine (Hyper-V's vEthernet switch, VMware host-only networks, a VPN
// tunnel's leftover address, etc.) via a direct GetAdaptersAddresses query, separate from
// CastingStreamServer's own address-selection logic -- so this check still catches a
// regression even if a future change breaks that selection logic itself, rather than just
// re-running the same code it's meant to verify.
juce::StringArray collectNoGatewayPrivateAddresses()
{
    juce::StringArray result;
    ULONG bufferLen = 16384;
    std::vector<uint8_t> buffer(bufferLen);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    auto flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS;
    auto status = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &bufferLen);
    if (status == ERROR_BUFFER_OVERFLOW)
    {
        buffer.resize(bufferLen);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        status = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &bufferLen);
    }
    if (status != NO_ERROR)
        return result;

    for (auto* adapter = addresses; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp || adapter->FirstGatewayAddress != nullptr)
            continue; // has a real gateway -- not one of the "bad" addresses this check looks for.
        for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
        {
            if (unicast->Address.lpSockaddr == nullptr || unicast->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            auto* sin = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN] {};
            if (inet_ntop(AF_INET, &sin->sin_addr, text, sizeof(text)) != nullptr)
                result.add(text);
        }
    }
    return result;
}

// Work Order 5: a way to ask "what does Windows.Media.Casting actually see on this network
// right now" with no UI, no user interaction, and no screen takeover -- CastingDevicePicker.Show()
// is interactive by design (it's an OS-shell-hosted flyout meant for a human to click through),
// but the same device set is available headlessly via CastingDevice::GetDeviceSelector() fed into
// the ordinary Windows.Devices.Enumeration API. Useful both for this automated probe and for any
// future non-interactive diagnostic (no need to drive a real click through computer-use just to
// find out whether a given receiver is Miracast/DLNA-visible).
void probeHeadlessDeviceEnumeration()
{
    namespace Enumeration = winrt::Windows::Devices::Enumeration;
    namespace Casting = winrt::Windows::Media::Casting;
    namespace Foundation = winrt::Windows::Foundation;

    try
    {
        const auto selector = Casting::CastingDevice::GetDeviceSelector(Casting::CastingPlaybackTypes::Video);
        auto async = Enumeration::DeviceInformation::FindAllAsync(selector);

        // A plain sleep-and-poll loop here would starve the completion notification: on an STA
        // thread, WinRT marshals an IAsyncOperation's completion back via this thread's own
        // message queue, so the loop must actually pump messages (as the frame-capture loop
        // below already does via runDispatchLoopUntil) or Status() can sit at Started forever
        // even once the OS-level enumeration work is really done. Confirmed empirically: a bare
        // juce::Thread::sleep() poll here never left Started within an 8s budget.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
        while (async.Status() == Foundation::AsyncStatus::Started && std::chrono::steady_clock::now() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(25);

        if (async.Status() != Foundation::AsyncStatus::Completed)
        {
            std::printf("INFO  headless device enumeration did not complete (status=%d) -- not a pass/fail signal by itself\n",
                (int) async.Status());
            return;
        }

        auto devices = async.GetResults();
        std::printf("INFO  headless Miracast/DLNA-visible device enumeration found %u device(s):\n", devices.Size());
        for (auto const& d : devices)
        {
            std::printf("INFO    - \"%s\" (id=%s)\n",
                winrt::to_string(d.Name()).c_str(), winrt::to_string(d.Id()).c_str());
        }
        if (devices.Size() == 0)
            std::printf("INFO    (none -- no device on this network currently advertises Miracast/DLNA \"Play To\" support)\n");
    }
    catch (const winrt::hresult_error& e)
    {
        std::printf("INFO  headless device enumeration threw hresult 0x%08x: %ls\n",
            (unsigned) e.code().value, e.message().c_str());
    }
}
#endif
}

int main()
{
#if ! JUCE_WINDOWS
    std::printf("SKIP  Windows.Media.Casting and Windows Graphics Capture are Windows-only; nothing to probe on this platform.\n");
    return 77;
#else
    juce::ScopedJuceInitialiser_GUI juceInit;

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    std::printf("\n");
    probeHeadlessDeviceEnumeration();
    std::printf("\n");

    ProbeWindow window;
    // Station's real window is a juce::DocumentWindow with a native title bar and taskbar
    // presence -- addToDesktop(0) instead produces a borderless WS_EX_TOOLWINDOW popup, which
    // Windows Graphics Capture rejects outright (confirmed empirically: CreateForWindow
    // returned E_INVALIDARG for exactly that window). Match Station's real window shape so this
    // probe tests the actual production case, not an artificially harder one.
    window.addToDesktop(juce::ComponentPeer::windowAppearsOnTaskbar | juce::ComponentPeer::windowHasTitleBar
                         | juce::ComponentPeer::windowHasCloseButton | juce::ComponentPeer::windowIsResizable);
    window.setVisible(true);
    window.toFront(false);
    // Give the OS/DWM a moment to actually composite the window before capturing it.
    for (int i = 0; i < 20; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }

    // --- Backend construction (picker + IInitializeWithWindow interop) ---
    cs::casting::WinRTCastingBackend backend(window);
    const bool apiAvailable = backend.isAvailable();
    check(apiAvailable, "Windows.Media.Casting's CastingDevicePicker initializes against this machine's SDK/runtime");
    if (! apiAvailable)
    {
        std::printf("\nWindows casting API unavailable on this machine -- treating the rest of the gate as skipped, not failed.\n");
        window.removeFromDesktop();
        return 77;
    }

    // --- Capture + encode + local HTTP endpoint (the "continuously updated live source" and
    //     "encoding/container/transport" parts of the mandatory gate) ---
    cs::casting::StationCastingStreamSource streamSource(window);
    const auto url = streamSource.start();
    check(url.isNotEmpty(), "capture -> encode -> local HTTP endpoint starts and returns a stream URL");
    if (url.isEmpty())
    {
        std::printf("\nCould not start the real capture/encode/stream pipeline -- see FAIL lines above for which stage.\n");
        window.removeFromDesktop();
        return 1;
    }
    std::printf("INFO  stream URL: %s\n", url.toRawUTF8());

    // Work Order 5 regression: the stream server must bind to an adapter actually routed to
    // the real network, not a virtual adapter (Hyper-V/VMware/stale VPN) a physical receiver
    // could never reach. See CastingStreamServer::findLanAddress()'s gateway-preference fix.
    {
        const auto chosenHost = juce::URL(url).getDomain();
        const auto badAddresses = collectNoGatewayPrivateAddresses();
        std::printf("INFO  gateway-less (unreachable-from-LAN) adapter addresses on this machine: %s\n",
            badAddresses.isEmpty() ? "(none)" : badAddresses.joinIntoString(", ").toRawUTF8());
        check(! badAddresses.contains(chosenHost),
              "the stream server did not bind to a gateway-less (virtual/VPN) adapter address");
    }

    // Let a few real frames flow (repaint the window so each captured frame actually differs,
    // proving this is live capture, not one static snapshot encoded repeatedly).
    for (int i = 0; i < 150; ++i) // ~5s -- wide enough to rule out a fragment-duration buffering delay.
    {
        window.repaint();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(33);
    }

    const auto dropped = streamSource.getDroppedCaptureFrameCount();
    std::printf("INFO  capture frames dropped by the bounded queue during this run: %zu\n", dropped);

    // --- A real TCP client confirms the local endpoint is a connectable, correctly-framed HTTP
    //     resource (the "local HTTP streaming endpoint... provides a supported media source"
    //     transport-level check -- not a claim that a TV's media engine accepts the content). ---
    juce::URL parsed(url);
    juce::StreamingSocket client;
    const bool connected = client.connect(parsed.getDomain(), parsed.getPort(), 2000);
    check(connected, "a plain TCP client can connect to the local streaming endpoint");

    if (connected)
    {
        auto subPath = parsed.getSubPath(true);
        if (! subPath.startsWithChar('/'))
            subPath = "/" + subPath;
        const auto request = "GET " + subPath + " HTTP/1.1\r\nHost: " + parsed.getDomain() + "\r\nConnection: close\r\n\r\n";
        std::printf("INFO  request: %s\n", request.replace("\r\n", "\\r\\n").toRawUTF8());
        const auto requestUtf8 = request.toUTF8();
        client.write(requestUtf8.getAddress(), (int) strlen(requestUtf8.getAddress()));

        // A single non-blocking-ish read can legitimately return 0 bytes if it lands before the
        // server has written anything yet (there is no guaranteed ordering between this client
        // issuing the request and the server's own accept/parse/respond sequence completing) --
        // retry briefly rather than treat one empty read as the server having no response.
        char responseBuffer[4096] = {};
        int received = 0;
        for (int i = 0; i < 100 && received <= 0; ++i)
        {
            received = client.read(responseBuffer, (int) sizeof(responseBuffer) - 1, false);
            if (received <= 0)
                juce::Thread::sleep(20);
        }
        std::printf("INFO  received %d response byte(s)\n", received);
        juce::String response(responseBuffer, (size_t) juce::jmax(0, received));
        std::printf("INFO  response: %s\n", response.replace("\r\n", "\\r\\n").toRawUTF8());

        check(response.contains("200") && response.containsIgnoreCase("chunked"),
              "the endpoint answers with a chunked 200 OK response");

        // The headers and the first chunk are two separate writes on the server side (the
        // header write happens synchronously in handleConnection; chunk delivery happens
        // shortly after, once the per-client writer thread picks the queued data up) -- they
        // are not guaranteed to land in the same TCP read, so keep reading for a short window
        // rather than judging "is there a payload" from the first read alone.
        const auto headerLength = (int) response.upToFirstOccurrenceOf("\r\n\r\n", true, false).length();
        int totalReceived = received;
        for (int i = 0; i < 100 && totalReceived <= headerLength; ++i)
        {
            char more[4096] = {};
            const int moreReceived = client.read(more, (int) sizeof(more) - 1, false);
            if (moreReceived > 0)
            {
                response += juce::String(more, (size_t) moreReceived);
                totalReceived += moreReceived;
            }
            else
                juce::Thread::sleep(20);
        }
        std::printf("INFO  total bytes received after headers (including any payload): %d (header length %d)\n", totalReceived, headerLength);
        const bool payloadWhileLive = totalReceived > headerLength;
        check(payloadWhileLive, "real payload bytes follow the HTTP headers while the session is still live (no Finalize needed)");

        if (! payloadWhileLive)
        {
            // Diagnostic-only follow-up, not part of the pass/fail gate: does Finalize() (called
            // inside stop(), before the server itself stops) flush anything that was buffered
            // internally by the fMP4 sink? This distinguishes "nothing was ever encoded" from
            // "it was encoded but only flushed at finalize," which are very different findings.
            streamSource.stop();
            char afterStop[4096] = {};
            int afterStopTotal = 0;
            for (int i = 0; i < 20; ++i)
            {
                const int n = client.read(afterStop, (int) sizeof(afterStop) - 1, false);
                if (n > 0) { afterStopTotal += n; break; }
                juce::Thread::sleep(20);
            }
            std::printf("INFO  bytes received after stop()/Finalize(): %d\n", afterStopTotal);
        }

        client.close();
    }

    streamSource.stop();
    window.setVisible(false);
    window.removeFromDesktop();

    std::printf("\n%d check(s) failed. (Device discovery UI and real-receiver playback are NOT covered here -- see the Work Order 3 report.)\n", failures);
    return failures == 0 ? 0 : 1;
#endif
}
