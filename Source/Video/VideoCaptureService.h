#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include <functional>
#include <vector>

namespace cs
{

struct VideoCaptureDeviceInfo
{
    juce::String id;
    juce::String name;
    bool isScreen = false;
};

class VideoCaptureService
{
public:
    VideoCaptureService();
    ~VideoCaptureService();

    std::vector<VideoCaptureDeviceInfo> getAvailableDevices() const;
    bool startRecording(const juce::String& deviceId, const juce::File& outputFile, juce::String& errorMessage);
    void setPreviewCallback(std::function<void(juce::Image)> callback);
    void stopRecording();
    bool isRecording() const;
    std::vector<juce::File> getRecordingFiles() const;
    void clearRecordingFiles();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace cs
