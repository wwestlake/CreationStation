#pragma once

#include <JuceHeader.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace cs
{
// Decodes video thumbnails off the message thread and caches the results, so paint() never
// blocks on VideoDecodeService's (deliberately blocking) decode calls. Uses exactly one
// background thread rather than a real pool: every VideoDecodeService instance shares one
// process-wide D3D11 immediate context (see VideoDecodeService.cpp), and immediate-context calls
// aren't safe to issue concurrently from multiple threads, so decode work has to be serialized
// regardless of how many worker threads existed.
class VideoThumbnailCache
{
public:
    VideoThumbnailCache() = default;
    // Not = default: member order below is deliberate (pool declared last, so it's destroyed
    // first) specifically so this can rely on that ordering rather than needing its own body.
    ~VideoThumbnailCache() = default;

    VideoThumbnailCache(const VideoThumbnailCache&) = delete;
    VideoThumbnailCache& operator=(const VideoThumbnailCache&) = delete;

    // Returns the cached thumbnail for cacheKey if one is ready (an invalid/null juce::Image
    // otherwise, in which case a background decode is queued unless one's already pending for
    // this key - a failed decode is cached too, as a permanently-invalid Image, so a broken file
    // doesn't get retried every repaint). onReady fires on the message thread once the decode
    // completes; it may fire after the result no longer matters (clip deleted/re-trimmed since),
    // so callers should re-check their own state rather than assume the thumbnail still applies.
    juce::Image getThumbnail(const juce::String& cacheKey, const juce::File& file, double sourceSeconds,
                             std::function<void()> onReady);

private:
    juce::CriticalSection lock;
    std::unordered_map<juce::String, juce::Image> cache;
    std::unordered_set<juce::String> pending;
    // Declared last: members are destroyed in reverse declaration order, so this - and the
    // guaranteed wait-for-running-jobs-to-finish in ThreadPool's own destructor - runs BEFORE
    // cache/pending/lock get torn down, closing the window for a job to touch them mid-teardown.
    juce::ThreadPool pool { 1 };
};
}
