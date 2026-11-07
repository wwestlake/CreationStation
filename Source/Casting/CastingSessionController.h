#pragma once

#include <functional>
#include <memory>
#include <juce_graphics/juce_graphics.h>
#include "CastingBackend.h"
#include "CastingStreamSource.h"
#include "CastingTypes.h"

namespace cs::casting
{

// Orchestrates one Cast-to-TV session: drives a CastingBackend's discovery/connect/disconnect
// state machine, starts/stops a CastingStreamSource at the right moments, and guarantees that
// every capture/encode/network resource the stream source acquired is released whenever the
// session ends -- normally, on failure, or on a receiver stall -- never left running.
//
// Owns neither the backend nor the stream source (both are injected): production wiring
// (MainComponent) supplies WinRTCastingBackend + the real capture/encode/server pipeline;
// tests/CastingSessionSmoke.cpp supplies FakeCastingBackend + a trivial fake stream source, so
// this class's own logic -- not Windows, not WinRT, not a TV -- is what gets exercised there.
class CastingSessionController
{
public:
    // Neither pointer may be null, and both must outlive this controller.
    CastingSessionController(CastingBackend& backendIn, CastingStreamSource& streamSourceIn);
    ~CastingSessionController();

    // Mirrors CastingBackend's state, plus the lifecycle decisions this class makes on top of
    // it (starting/stopping the stream source). This is what the UI should bind to.
    std::function<void(CastState)> onStateChanged;
    std::function<void(const CastError&)> onError;

    // Opens device discovery, anchored near the given screen rectangle. Only meaningful from
    // idle/disconnected/failed; ignored (no-op, no state change) if a session is already
    // active, so a confused double-click can never corrupt the state machine.
    void beginCast(juce::Rectangle<int> anchorScreenBounds);

    // Ends the current session (any state other than idle). No-op from idle.
    void disconnect();

    CastState getState() const noexcept { return state; }
    const CastDevice& getConnectedDevice() const noexcept { return connectedDevice; }

private:
    void handleDeviceSelected(const CastDevice& device);
    void handleBackendStateChanged(CastState backendState);
    void handleBackendError(const CastError& error);
    void releaseStreamSourceIfRunning();

    CastingBackend& backend;
    CastingStreamSource& streamSource;
    CastState state = CastState::idle;
    CastDevice connectedDevice;
    bool streamSourceRunning = false;

    JUCE_DECLARE_NON_COPYABLE(CastingSessionController)
};

} // namespace cs::casting
