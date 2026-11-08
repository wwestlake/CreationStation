#pragma once

#include <juce_core/juce_core.h>

// Shared vocabulary for the Cast-to-TV feature (Work Order 3). Kept free of any OS/WinRT
// dependency so it can be included by both the real backend and the tests.
namespace cs::casting
{

// The full lifecycle a cast session moves through. UI code only ever needs to react to this,
// never to backend-specific detail.
enum class CastState
{
    idle,        // Not casting. No capture, encoder, or server resources held.
    searching,   // Device picker is open / discovery in progress.
    connecting,  // A receiver was chosen; establishing the casting connection.
    connected,   // Live: capture -> encode -> stream -> receiver is running.
    disconnecting,
    disconnected, // Ended normally (user disconnect, or receiver closed the session).
    failed        // Ended abnormally; see CastError for why.
};

inline juce::String castStateToString(CastState state)
{
    switch (state)
    {
        case CastState::idle:          return "Idle";
        case CastState::searching:     return "Searching for devices";
        case CastState::connecting:    return "Connecting";
        case CastState::connected:     return "Connected";
        case CastState::disconnecting: return "Disconnecting";
        case CastState::disconnected:  return "Disconnected";
        case CastState::failed:        return "Connection failed";
    }
    return "Unknown";
}

// Why a session ended or failed to start. Deliberately a short, closed set rather than a raw
// string: the UI needs to choose different user-facing wording per cause, and tests need to
// assert on cause, not on message text that might be edited later.
enum class CastErrorCode
{
    none,
    noReceiverSelected,      // User dismissed the device picker without choosing anything.
    castingApiUnavailable,   // The OS casting/media-casting stack isn't available on this machine.
    captureUnavailable,      // Window Graphics Capture isn't supported/available for our window.
    encoderUnavailable,      // No usable H.264 encoder could be configured.
    streamServerFailed,      // Couldn't bind/listen the local stream endpoint.
    connectionRejected,      // The receiver refused or dropped the casting connection.
    receiverStalled,         // The receiver stopped consuming the stream (timeout).
    internalError
};

struct CastError
{
    CastErrorCode code = CastErrorCode::none;
    juce::String detail; // Short, human-readable elaboration (never a stack trace / raw HRESULT text to the UI).

    bool isSet() const noexcept { return code != CastErrorCode::none; }
};

juce::String describeCastError(const CastError& error);

// One discovered or chosen receiver. The real backend fills this in from whatever the OS
// picker/connection reports; the fake backend used in tests constructs these directly.
struct CastDevice
{
    juce::String id;          // Backend-defined stable id (not shown to the user).
    juce::String displayName; // What the user sees ("Living Room TV").
};

} // namespace cs::casting
