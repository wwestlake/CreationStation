#pragma once

#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

namespace creation::assets
{
class VirtualFileSystem final
{
public:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

    bool mount(const juce::File& zipFile, int priority = 0);
    void unmountAll();

    bool exists(const juce::String& virtualPath) const;
    bool readFile(const juce::String& virtualPath, juce::MemoryBlock& outData) const;

private:
    struct MountedArchive
    {
        std::unique_ptr<juce::ZipFile> zip;
        int priority = 0;
        juce::File sourceFile;
    };

    static juce::String normalizePath(const juce::String& virtualPath);

    mutable juce::CriticalSection lock;
    std::vector<MountedArchive> mounts;
};
}
