#include "StationCastingStreamSource.h"

#include <cstdio>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace cs::casting
{

StationCastingStreamSource::StationCastingStreamSource(juce::Component& windowToCapture)
    : window(windowToCapture)
{
}

StationCastingStreamSource::~StationCastingStreamSource()
{
    stop();
}

juce::String StationCastingStreamSource::start()
{
#if JUCE_WINDOWS
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return {};
    auto handle = peer->getNativeHandle();

    if (! capture.start(handle))
    {
        fprintf(stderr, "StationCastingStreamSource: capture.start() failed\n");
        return {};
    }

    // Wait briefly for the capture's first real frame so the encoder can be configured with the
    // exact pixel dimensions Windows Graphics Capture actually reports for this window, rather
    // than guessing from JUCE's own (DPI-scaled) component size.
    CapturedFrame firstFrame;
    bool gotFirstFrame = false;
    for (int i = 0; i < 400 && ! gotFirstFrame; ++i) // up to ~2s
    {
        if (capture.tryPopFrame(firstFrame))
            gotFirstFrame = true;
        else
            juce::Thread::sleep(5);
    }
    if (! gotFirstFrame)
    {
        fprintf(stderr, "StationCastingStreamSource: no frame arrived within the wait budget\n");
        capture.stop();
        return {};
    }

    const int width = firstFrame.image.getWidth();
    const int height = firstFrame.image.getHeight();

    // Must be set before encoder.start(): CastingMp4Sink::start() reads this member once and
    // copies it into the underlying byte stream's callback at that moment -- assigning it
    // afterwards would silently leave the byte stream holding an empty (no-op) callback, since
    // the copy is not a live reference back to this member. (Found exactly this way: WriteSample
    // kept succeeding but no bytes ever reached the server, because onBytesProduced really was
    // still empty at the moment encoder.start() read it.)
    encoder.onBytesProduced = [this](const void* data, size_t numBytes) { server.pushBytes(data, numBytes); };

    if (! encoder.start(width, height, 30.0))
    {
        fprintf(stderr, "StationCastingStreamSource: encoder.start(%d, %d) failed\n", width, height);
        capture.stop();
        return {};
    }

    const auto url = server.start();
    if (url.isEmpty())
    {
        fprintf(stderr, "StationCastingStreamSource: server.start() failed (no LAN address or no port could be bound)\n");
        encoder.stop();
        capture.stop();
        return {};
    }

    // The first frame was already popped off the queue above to learn its size -- encode it now
    // so it isn't silently lost.
    encoder.encodeFrame(firstFrame.image, firstFrame.captureTimeSeconds);

    stopRequested = false;
    running = true;
    encodeThread = std::thread([this] { encodeLoop(); });

    return url;
#else
    return {};
#endif
}

void StationCastingStreamSource::encodeLoop()
{
    while (! stopRequested.load())
    {
        CapturedFrame frame;
        if (capture.tryPopFrame(frame))
            encoder.encodeFrame(frame.image, frame.captureTimeSeconds);
        else
            juce::Thread::sleep(5);
    }
}

void StationCastingStreamSource::stop()
{
    stopRequested = true;
    if (encodeThread.joinable())
        encodeThread.join();
    encoder.stop();
    capture.stop();
    server.stop();
    running = false;
}

} // namespace cs::casting
