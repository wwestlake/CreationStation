#pragma once

#include <functional>
#include <juce_graphics/juce_graphics.h>
#include "CastingTypes.h"

namespace cs::casting
{

// Everything CastingSessionController needs from "the thing that actually talks to a TV."
// Exists so the session's state machine, UI wiring, and lifecycle/error-handling logic can be
// exercised in tests without the real OS casting stack (WinRTCastingBackend) or any hardware --
// see FakeCastingBackend. Only one implementation (WinRTCastingBackend) talks to Windows.
class CastingBackend
{
public:
    virtual ~CastingBackend() = default;

    // Fired whenever the backend's own view of the session state changes. The controller is the
    // only intended listener; it owns translating this into UI state.
    std::function<void(CastState)> onStateChanged;

    // Fired when a session ends abnormally or fails to start. Always preceded or followed by an
    // onStateChanged(failed) or onStateChanged(disconnected) call.
    std::function<void(const CastError&)> onError;

    // Fired once the user has chosen a receiver from discovery (or dismissed it, in which case
    // this is never fired and onStateChanged(idle) / onError(noReceiverSelected) follows).
    std::function<void(const CastDevice&)> onDeviceSelected;

    // Opens device discovery/selection, anchored near the given screen rectangle (so a native
    // picker opens next to the Cast button, the way a browser's own Cast button does). Must
    // return immediately; never blocks the calling (UI) thread.
    virtual void beginDiscovery(juce::Rectangle<int> anchorScreenBounds) = 0;

    // Cancels discovery if currently showing/searching. No-op otherwise.
    virtual void cancelDiscovery() = 0;

    // Begins casting the given live stream URL to the most recently selected device. Must
    // return immediately; progress and outcome arrive via onStateChanged/onError.
    virtual void startCasting(const juce::String& streamUrl) = 0;

    // Ends the current session if one exists. Safe to call from idle. Must not block for more
    // than a few milliseconds -- callers include UI shutdown paths.
    virtual void stopCasting() = 0;
};

} // namespace cs::casting
