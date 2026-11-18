#pragma once

#include <functional>
#include <memory>
#include <juce_graphics/juce_graphics.h>

namespace cs::casting
{

// Encodes captured frames to H.264 video, muxed live into a fragmented MP4 (fMP4) byte stream
// via Windows Media Foundation's MFCreateFMPEG4MediaSink -- the same OS-provided muxer
// Microsoft documents for authoring fragmented MP4 (the format a live/indefinite-length stream
// needs: a classic, non-fragmented MP4's index is normally written only once, at the end, which
// doesn't exist for a cast session with no fixed end). No third-party encoding library is used;
// this is entirely Media Foundation, which already ships with Windows and is already used
// elsewhere in this suite's codebase (Djehuti Movie's video decode path).
//
// The resulting bytes (first the fMP4 initialization segment, then a continuous sequence of
// movie fragments) are handed to onBytesProduced as they're written -- CastingStreamServer is
// the intended consumer, turning them into a chunked HTTP response for a receiver. This class
// knows nothing about networking.
class CastingMp4Sink
{
public:
    CastingMp4Sink();
    ~CastingMp4Sink();

    // Invoked synchronously from inside encodeFrame() whenever the muxer writes new bytes.
    // Must return quickly and must not block -- it runs on the single thread that calls
    // encodeFrame(), which must not be the UI or audio thread.
    std::function<void(const void* data, size_t numBytes)> onBytesProduced;

    // Configures and starts the encoder for the given frame size/rate. Returns false (nothing
    // left running) if no usable H.264 encoder could be found on this machine.
    bool start(int frameWidth, int frameHeight, double frameRate);

    // Encodes one BGRA frame. captureTimeSeconds must increase from call to call; it becomes
    // the fMP4 sample timestamp. Returns false if the frame couldn't be encoded (the caller
    // should treat repeated failures as the stream having gone bad, not retry forever).
    bool encodeFrame(const juce::Image& bgraImage, double captureTimeSeconds);

    // Flushes trailing fragments and releases the encoder/muxer. Safe to call even if start()
    // was never called or already failed.
    void stop();

    bool isRunning() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(CastingMp4Sink)
};

} // namespace cs::casting
