#pragma once

#include <JuceHeader.h>
#include <functional>
#include "VideoDecodeService.h"

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

    void requestFrame(const juce::File& file, double sourceSeconds, std::function<void(juce::Image)> onFrameReady)
    {
        const juce::ScopedLock sl(lock);
        pendingFile = file;
        pendingSourceSeconds = sourceSeconds;
        pendingCallback = std::move(onFrameReady);
        hasPendingRequest = true;

        if (! jobRunning)
            launchNextJob();
    }

private:
    // Caller must hold lock.
    void launchNextJob()
    {
        if (! hasPendingRequest)
            return;

        auto file = pendingFile;
        auto sourceSeconds = pendingSourceSeconds;
        auto callback = pendingCallback;
        hasPendingRequest = false;
        jobRunning = true;

        pool.addJob([this, file, sourceSeconds, callback]
        {
            // Real bug fixed here: this used to construct a fresh VideoDecodeService and call
            // open() from scratch on every single tick -- a full Media Foundation source-reader
            // creation (container probe, codec negotiation) many times a second, for what's
            // almost always the same clip the playhead is still sitting over. Other tools open a
            // file once and just seek; this now does the same -- openService/openFile persist
            // across calls, and open() only runs again when the scrubbed clip actually changes.
            if (openFile != file || ! openService.isOpen())
            {
                openValid = openService.open(file).valid;
                openFile = file;
            }

            juce::Image decoded;
            if (openValid)
                decoded = openService.decodeFrameAt(sourceSeconds, 320, 180);

            if (callback)
                juce::MessageManager::callAsync([callback, decoded] { callback(decoded); });

            const juce::ScopedLock sl(lock);
            jobRunning = false;
            launchNextJob();
        });
    }

    juce::CriticalSection lock;
    bool jobRunning = false;
    bool hasPendingRequest = false;
    juce::File pendingFile;
    double pendingSourceSeconds = 0.0;
    std::function<void(juce::Image)> pendingCallback;
    // Touched only from the ThreadPool's one worker thread (never the message thread), so no
    // lock needed for these despite jobRunning/hasPendingRequest above needing one.
    VideoDecodeService openService;
    juce::File openFile;
    bool openValid = false;
    // Declared last so it's destroyed (and its guaranteed wait-for-running-job semantics run)
    // before the state above, closing the same teardown race VideoThumbnailCache avoids.
    juce::ThreadPool pool { 1 };
};
}
