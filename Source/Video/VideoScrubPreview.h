#pragma once

#include <JuceHeader.h>
#include <functional>
#include "VideoDecodeService.h"
#include "VideoSource.h"

namespace cs
{
// Drives the Tracker's video-track scrub preview: unlike VideoThumbnailCache (many distinct
// clips, cache forever), this is one continuously-moving playhead position - there's nothing
// useful to cache, and requesting a fresh decode every single timer tick would just pile up a
// growing queue of stale positions behind the one background decode thread (VideoDecodeService
// instances share a process-wide D3D11 immediate context, so decode work is serialized - see
// VideoDecodeService.cpp). Instead this keeps only the MOST RECENT request: if a new one arrives
// while a decode is already running, the running one finishes and hands back whatever it has,
// then immediately starts on the latest request rather than anything queued in between.
class VideoScrubPreview
{
public:
    VideoScrubPreview() = default;
    ~VideoScrubPreview() = default;

    VideoScrubPreview(const VideoScrubPreview&) = delete;
    VideoScrubPreview& operator=(const VideoScrubPreview&) = delete;

    // maxWidth/maxHeight: the biggest picture wanted (the view's pixel size); it is never decoded larger than the
    // video itself, since that would only stretch the picture and cost time.
    void requestFrame(const VideoSource& source, double sourceSeconds, std::function<void(juce::Image)> onFrameReady,
                      int maxWidth = 320, int maxHeight = 180)
    {
        const juce::ScopedLock sl(lock);
        pendingSource = source;
        pendingSourceSeconds = sourceSeconds;
        pendingWidth = maxWidth;
        pendingHeight = maxHeight;
        pendingCallback = std::move(onFrameReady);
        hasPendingRequest = true;

        if (! jobRunning)
            launchNextJob();
    }

    // Why the last decode produced no picture ("" when it did). Safe from any thread.
    juce::String getLastError() const
    {
        const juce::ScopedLock sl(lock);
        return lastDecodeError;
    }

private:
    void setLastError(const juce::String& text)
    {
        const juce::ScopedLock sl(lock);
        lastDecodeError = text;
    }

    // Caller must hold lock.
    void launchNextJob()
    {
        if (! hasPendingRequest)
            return;

        auto source = pendingSource;
        auto sourceSeconds = pendingSourceSeconds;
        const auto wantWidth = pendingWidth;
        const auto wantHeight = pendingHeight;
        auto callback = pendingCallback;
        hasPendingRequest = false;
        jobRunning = true;

        pool.addJob([this, source, sourceSeconds, callback, wantWidth, wantHeight]
        {
            // Real bug fixed here: this used to construct a fresh VideoDecodeService and call
            // open() from scratch on every single tick -- a full Media Foundation source-reader
            // creation (container probe, codec negotiation) many times a second, for what's
            // almost always the same clip the playhead is still sitting over. Other tools open a
            // file once and just seek; this now does the same -- openService/openFile persist
            // across calls, and open() only runs again when the scrubbed clip actually changes.
            if (! openSource.sameVideoAs(source) || ! openService.isOpen())
            {
                if (source.makeStream)
                {
                    auto stream = source.makeStream();
                    openInfo = stream != nullptr ? openService.open(std::move(stream), source.nameHint) : VideoStreamInfo {};
                }
                else
                {
                    openInfo = openService.open(source.file);
                }
                openValid = openInfo.valid;
                openSource = source;
                if (! openValid)
                    setLastError("could not open the video: " + openService.getLastError());
            }

            juce::Image decoded;
            if (openValid)
                decoded = openService.decodeFrameAt(sourceSeconds,
                                                    juce::jlimit(16, juce::jmax(16, openInfo.width), wantWidth),
                                                    juce::jlimit(16, juce::jmax(16, openInfo.height), wantHeight));

            if (openValid)
                setLastError(decoded.isValid() ? juce::String() : openService.getLastError());

            if (callback)
                juce::MessageManager::callAsync([callback, decoded] { callback(decoded); });

            const juce::ScopedLock sl(lock);
            jobRunning = false;
            launchNextJob();
        });
    }

    mutable juce::CriticalSection lock;
    juce::String lastDecodeError;
    bool jobRunning = false;
    bool hasPendingRequest = false;
    VideoSource pendingSource;
    double pendingSourceSeconds = 0.0;
    int pendingWidth = 320;
    int pendingHeight = 180;
    std::function<void(juce::Image)> pendingCallback;
    // Touched only from the ThreadPool's one worker thread (never the message thread), so no
    // lock needed for these despite jobRunning/hasPendingRequest above needing one.
    VideoDecodeService openService;
    VideoSource openSource;
    bool openValid = false;
    VideoStreamInfo openInfo;
    // Declared last so it's destroyed (and its guaranteed wait-for-running-job semantics run)
    // before the state above, closing the same teardown race VideoThumbnailCache avoids.
    juce::ThreadPool pool { 1 };
};
}
