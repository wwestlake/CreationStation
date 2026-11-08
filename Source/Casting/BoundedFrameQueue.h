#pragma once

#include <deque>
#include <mutex>

// A small bounded, drop-oldest queue. Used to carry frames from the capture thread to the
// encoder thread (and encoded chunks from the encoder thread to the network thread) without
// ever growing without limit: Work Order 3 requires that a slow receiver or network cannot
// cause unbounded memory growth, and that frame dropping is controlled rather than accidental.
//
// Deliberately plain and lock-based rather than lock-free: frame-queue traffic here runs at
// video frame rate (tens of pushes/pops per second), nowhere near the real-time audio thread's
// budget, so a short mutex hold is not a concern. It must never be used on the audio thread.
template <typename T>
class BoundedFrameQueue
{
public:
    explicit BoundedFrameQueue(size_t maxDepth) : capacity(maxDepth) {}

    // Pushes a new item. If the queue is already at capacity, the OLDEST item is discarded to
    // make room (never the newest -- a caster wants the freshest frame, not a backlog). Returns
    // true if an item had to be dropped to make room.
    bool push(T item)
    {
        std::lock_guard<std::mutex> lock(mutex);
        bool dropped = false;
        if (items.size() >= capacity)
        {
            items.pop_front();
            dropped = true;
            ++droppedCount;
        }
        items.push_back(std::move(item));
        return dropped;
    }

    // Pops the oldest item if one is present. Returns false (and leaves out untouched) if empty.
    bool tryPop(T& out)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (items.empty())
            return false;
        out = std::move(items.front());
        items.pop_front();
        return true;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.size();
    }

    size_t getCapacity() const noexcept { return capacity; }

    size_t getDroppedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return droppedCount;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        items.clear();
    }

private:
    mutable std::mutex mutex;
    std::deque<T> items;
    size_t capacity;
    size_t droppedCount = 0;
};
