// Headless checks for Work Order 3 (Cast to TV): CastingSessionController's device-discovery,
// connect/fail/disconnect/reconnect state machine (driven entirely through FakeCastingBackend
// and a fake stream source -- no Windows casting API, no Media Foundation, no real receiver),
// plus BoundedFrameQueue's bounded/drop-oldest behavior. Needs no hardware. Exit code 0 = pass.
//
// What this does NOT and cannot verify: that the real WinRT casting backend talks correctly to
// an actual TV, or that the real H.264/fragmented-MP4 stream plays on real receiver hardware.
// See tests/CastingFeasibilityProbe.cpp (Windows-only) for the real-API plumbing check, and the
// Work Order 3 report for what still needs a physical television.
#include <JuceHeader.h>

#include "Casting/BoundedFrameQueue.h"
#include "Casting/CastingSessionController.h"
#include "Casting/CastingStreamSource.h"
#include "Casting/FakeCastingBackend.h"

#include <cstdio>

namespace
{
int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

using namespace cs::casting;

// A CastingStreamSource that does no real capture/encode/network work, just counts calls and
// can be told to fail its next start() (simulating "the camera/encoder/socket couldn't start").
class FakeCastingStreamSource : public CastingStreamSource
{
public:
    juce::String start() override
    {
        ++startCallCount;
        if (failNextStart)
        {
            failNextStart = false;
            return {};
        }
        running = true;
        return "http://127.0.0.1:0/cast/fake-session";
    }

    void stop() override
    {
        ++stopCallCount;
        running = false;
    }

    bool failNextStart = false;
    bool running = false;
    int startCallCount = 0;
    int stopCallCount = 0;
};

struct Harness
{
    FakeCastingBackend backend;
    FakeCastingStreamSource streamSource;
    CastingSessionController controller { backend, streamSource };
    std::vector<CastState> observedStates;
    std::vector<CastError> observedErrors;

    Harness()
    {
        controller.onStateChanged = [this](CastState s) { observedStates.push_back(s); };
        controller.onError = [this](const CastError& e) { observedErrors.push_back(e); };
    }
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // 1. Happy path: discovery -> device selected -> stream source starts -> backend connects.
    {
        Harness h;
        h.controller.beginCast({ 100, 100, 40, 24 });
        check(h.backend.getState() == CastState::searching, "beginCast opens discovery");

        CastDevice device { "dev-1", "Living Room TV" };
        h.backend.simulateDeviceFound(device);
        check(h.streamSource.startCallCount == 1, "a device being selected starts the stream source");
        check(h.backend.getStartCastingCallCount() == 1, "the backend is asked to start casting after the stream source succeeds");
        check(h.backend.getLastStreamUrl().isNotEmpty(), "the backend receives a non-empty stream URL");

        h.backend.simulateConnected();
        check(h.controller.getState() == CastState::connected, "controller reaches connected");
        check(h.controller.getConnectedDevice().id == "dev-1", "controller remembers the connected device");
        check(h.streamSource.stopCallCount == 0, "the stream source is not stopped while connected");
    }

    // 2. Connection failure after a device was chosen: backend rejects/fails, stream source is
    //    released, no resources left running.
    {
        Harness h;
        h.controller.beginCast({});
        h.backend.simulateDeviceFound({ "dev-2", "Bedroom TV" });
        check(h.streamSource.startCallCount == 1, "stream source started before the failure");

        h.backend.simulateFailure({ CastErrorCode::connectionRejected, "test" });
        check(h.controller.getState() == CastState::failed, "controller reaches failed");
        check(h.streamSource.stopCallCount == 1, "the stream source is stopped on failure");
        check(! h.observedErrors.empty() && h.observedErrors.back().code == CastErrorCode::connectionRejected,
              "the error reaches the controller's listener with the right code");
    }

    // 3. Stream source itself fails to start (capture/encoder/server problem): the backend must
    //    never be asked to cast a stream that doesn't exist.
    {
        Harness h;
        h.streamSource.failNextStart = true;
        h.controller.beginCast({});
        h.backend.simulateDeviceFound({ "dev-3", "Kitchen TV" });

        check(h.backend.getStartCastingCallCount() == 0, "backend.startCasting is never called when the stream source fails");
        check(h.controller.getState() == CastState::failed, "controller reports failed when the stream source fails");
        check(h.streamSource.stopCallCount == 0, "a stream source that never started is not told to stop");
        check(! h.observedErrors.empty() && h.observedErrors.back().code == CastErrorCode::captureUnavailable,
              "the failure is reported as captureUnavailable");
    }

    // 4. Clean user-initiated disconnect while connected, then reconnect from scratch.
    {
        Harness h;
        h.controller.beginCast({});
        h.backend.simulateDeviceFound({ "dev-4", "Office TV" });
        h.backend.simulateConnected();
        check(h.controller.getState() == CastState::connected, "connected before disconnecting");

        h.controller.disconnect();
        check(h.controller.getState() == CastState::disconnected, "controller reaches disconnected after disconnect()");
        check(h.streamSource.stopCallCount == 1, "disconnect releases the stream source exactly once");

        // Reconnect: beginCast must work again from disconnected, not be ignored.
        h.controller.beginCast({});
        check(h.backend.getState() == CastState::searching, "beginCast works again after a disconnect");
        h.backend.simulateDeviceFound({ "dev-4", "Office TV" });
        h.backend.simulateConnected();
        check(h.controller.getState() == CastState::connected, "reconnect reaches connected");
        check(h.streamSource.startCallCount == 2, "the stream source is restarted for the new session");
    }

    // 5. A confused repeat click while already connecting/connected must be ignored, not corrupt
    //    the session (no second discovery, no second stream-source start).
    {
        Harness h;
        h.controller.beginCast({});
        h.backend.simulateDeviceFound({ "dev-5", "Garage TV" });
        check(h.controller.getState() == CastState::connecting, "connecting after device selection");

        h.controller.beginCast({}); // should be a no-op
        check(h.streamSource.startCallCount == 1, "a repeat beginCast while connecting does not start a second stream");

        h.backend.simulateConnected();
        h.controller.beginCast({}); // should also be a no-op while connected
        check(h.streamSource.startCallCount == 1, "a repeat beginCast while connected does not start a second stream");
    }

    // 6. Discovery dismissed with no device chosen: back to idle, nothing ever started.
    {
        Harness h;
        h.controller.beginCast({});
        h.backend.simulateDiscoveryDismissedWithNoSelection();
        check(h.controller.getState() == CastState::idle, "dismissing discovery with no choice returns to idle");
        check(h.streamSource.startCallCount == 0, "the stream source is never started if no device was chosen");
    }

    // 7. A receiver stall ends the session like any other failure and releases resources.
    {
        Harness h;
        h.controller.beginCast({});
        h.backend.simulateDeviceFound({ "dev-7", "Den TV" });
        h.backend.simulateConnected();
        h.backend.simulateFailure({ CastErrorCode::receiverStalled, "no response for 5s" });
        check(h.controller.getState() == CastState::failed, "a receiver stall is reported as a failed session");
        check(h.streamSource.stopCallCount == 1, "resources are released after a receiver stall");
    }

    // 8. Destroying the controller mid-session must still release the stream source -- an app
    //    shutdown or crash path must never leak capture/encoder/server resources.
    {
        FakeCastingBackend backend;
        FakeCastingStreamSource streamSource;
        {
            CastingSessionController controller { backend, streamSource };
            controller.beginCast({});
            backend.simulateDeviceFound({ "dev-8", "Guest TV" });
            backend.simulateConnected();
            check(streamSource.running, "stream source is running mid-session");
        }
        check(streamSource.stopCallCount == 1, "destroying the controller while connected still stops the stream source");
    }

    // 9. BoundedFrameQueue: bounded capacity, drop-oldest, FIFO order, drop counting.
    {
        BoundedFrameQueue<int> queue(3);
        check(! queue.push(1), "pushing into a non-full queue drops nothing");
        check(! queue.push(2), "pushing into a non-full queue drops nothing (2)");
        check(! queue.push(3), "pushing into a non-full queue drops nothing (3)");
        check(queue.size() == 3, "queue holds exactly its capacity");

        const bool dropped = queue.push(4);
        check(dropped, "pushing past capacity reports a drop");
        check(queue.size() == 3, "queue never exceeds its capacity");
        check(queue.getDroppedCount() == 1, "dropped-frame count is tracked");

        int value = 0;
        check(queue.tryPop(value) && value == 2, "the oldest surviving item (2, since 1 was dropped) comes out first");
        check(queue.tryPop(value) && value == 3, "FIFO order is preserved");
        check(queue.tryPop(value) && value == 4, "FIFO order is preserved (last item)");
        check(! queue.tryPop(value), "popping an empty queue fails cleanly");

        queue.push(10);
        queue.push(20);
        queue.clear();
        check(queue.size() == 0, "clear() empties the queue");
    }

    std::printf("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
