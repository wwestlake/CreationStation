#include "VideoThumbnailCache.h"
#include "VideoDecodeService.h"

namespace cs
{
juce::Image VideoThumbnailCache::getThumbnail(const juce::String& cacheKey, const juce::File& file, double sourceSeconds,
                                              std::function<void()> onReady)
{
    {
        const juce::ScopedLock sl(lock);
        auto existing = cache.find(cacheKey);
        if (existing != cache.end())
            return existing->second;

        if (pending.count(cacheKey) > 0)
            return {};

        pending.insert(cacheKey);
    }

    pool.addJob([this, cacheKey, file, sourceSeconds, onReady]
    {
        VideoDecodeService service;
        auto streamInfo = service.open(file);
        juce::Image decoded;
        if (streamInfo.valid)
            decoded = service.decodeFrameAt(sourceSeconds, 160, 90);

        {
            const juce::ScopedLock sl(lock);
            cache[cacheKey] = decoded;
            pending.erase(cacheKey);
        }

        if (onReady)
            juce::MessageManager::callAsync(onReady);
    });

    return {};
}
}
