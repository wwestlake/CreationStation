#pragma once

#include <juce_core/juce_core.h>

namespace cs::casting
{

// What CastingSessionController needs from "the thing that actually produces the live stream" --
// i.e. the real capture -> encode -> local HTTP server pipeline (see StationWindowCapture,
// CastingMp4Sink, CastingStreamServer). Abstracted out so the controller's own state-machine and
// lifecycle logic can be unit-tested (tests/CastingSessionSmoke.cpp) without touching Windows
// Graphics Capture, Media Foundation, or a real socket.
class CastingStreamSource
{
public:
    virtual ~CastingStreamSource() = default;

    // Starts capture, encoding, and the local stream server, and returns the URL a casting
    // backend should point a receiver at. Returns an empty string on failure (no resources are
    // left running in that case). Must not block the calling thread for long: real capture/
    // encoder/server startup happens on their own threads.
    virtual juce::String start() = 0;

    // Stops and fully releases every resource start() may have acquired (capture session,
    // encoder, listening socket). Safe to call even if start() was never called, already
    // failed, or was already stopped.
    virtual void stop() = 0;
};

} // namespace cs::casting
