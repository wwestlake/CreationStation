#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace cs
{
// Where a video comes from. A video in the project has no file on the disk: it is opened as a stream that reads it
// in pieces from the VFS (makeStream). `file` is only for a video that is not in the project (the user's own file, at
// the moment it is being imported). `key` says which video it is (the asset id): the same key is the same video.
struct VideoSource
{
    juce::String key;
    juce::File file;
    std::function<std::unique_ptr<juce::InputStream>()> makeStream;
    juce::String nameHint; // the original file name with its extension, so Windows can recognise the container

    bool isValid() const { return file != juce::File() || static_cast<bool>(makeStream); }
    bool sameVideoAs(const VideoSource& other) const { return key == other.key && file == other.file; }
};
}
