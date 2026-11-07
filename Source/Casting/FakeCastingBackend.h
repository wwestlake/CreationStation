#pragma once

#include "CastingBackend.h"

namespace cs::casting
{

// A CastingBackend driven entirely by direct calls from a test -- no OS casting stack, no
// timers, no threads. Lets CastingSessionController's state machine, error handling, and
// disconnect/reconnect logic be exercised deterministically without Windows, WinRT, or a
// physical receiver. See tests/CastingSessionSmoke.cpp.
class FakeCastingBackend : public CastingBackend
{
public:
    // -- CastingBackend --
    void beginDiscovery(juce::Rectangle<int>) override
    {
        discoveryOpen = true;
        setState(CastState::searching);
    }

    void cancelDiscovery() override
    {
        if (! discoveryOpen)
            return;
        discoveryOpen = false;
        setState(CastState::idle);
    }

    void startCasting(const juce::String& url) override
    {
        discoveryOpen = false;
        lastStreamUrl = url;
        ++startCastingCallCount;
        setState(CastState::connecting);
    }

    void stopCasting() override
    {
        if (state == CastState::idle)
            return;
        ++stopCastingCallCount;
        setState(CastState::disconnecting);
        setState(CastState::disconnected);
    }

    // -- Test driving surface --

    // Simulates the user choosing a device from the (fake) picker. Only meaningful while
    // discovery is open; the controller is expected to respond by calling startCasting().
    void simulateDeviceFound(const CastDevice& device)
    {
        jassert(discoveryOpen);
        if (onDeviceSelected)
            onDeviceSelected(device);
    }

    void simulateDiscoveryDismissedWithNoSelection()
    {
        discoveryOpen = false;
        setState(CastState::idle);
    }

    void simulateConnected()
    {
        jassert(state == CastState::connecting);
        setState(CastState::connected);
    }

    void simulateFailure(CastError error)
    {
        if (onError)
            onError(error);
        setState(CastState::failed);
    }

    void simulateDisconnectedByReceiver()
    {
        setState(CastState::disconnected);
    }

    CastState getState() const noexcept { return state; }
    const juce::String& getLastStreamUrl() const noexcept { return lastStreamUrl; }
    int getStartCastingCallCount() const noexcept { return startCastingCallCount; }
    int getStopCastingCallCount() const noexcept { return stopCastingCallCount; }

private:
    void setState(CastState newState)
    {
        state = newState;
        if (onStateChanged)
            onStateChanged(state);
    }

    CastState state = CastState::idle;
    bool discoveryOpen = false;
    juce::String lastStreamUrl;
    int startCastingCallCount = 0;
    int stopCastingCallCount = 0;
};

} // namespace cs::casting
