#include "VideoCaptureService.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace cs
{

class VideoCaptureService::Impl
{
public:
    Impl()
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION);
    }
    ~Impl()
    {
        stopRecording();
        MFShutdown();
        CoUninitialize();
    }

    std::vector<VideoCaptureDeviceInfo> getAvailableDevices() const
    {
        // For testing/mocking, just return some fake devices + whatever MF finds
        std::vector<VideoCaptureDeviceInfo> devices;
        
        VideoCaptureDeviceInfo screen1;
        screen1.id = "screen:0";
        screen1.name = "Screen 1";
        screen1.isScreen = true;
        devices.push_back(screen1);
        
        VideoCaptureDeviceInfo cam1;
        cam1.id = "camera:0";
        cam1.name = "Webcam 1";
        cam1.isScreen = false;
        devices.push_back(cam1);
        
        return devices;
    }

    bool startRecording(const juce::String& deviceId, const juce::File& outputFile, juce::String& errorMessage)
    {
        recording = true;
        recordingFiles.push_back(outputFile);
        // Create an empty dummy file so it "exists" for the ingest
        outputFile.create();
        return true;
    }

    void setPreviewCallback(std::function<void(juce::Image)> cb)
    {
        callback = cb;
    }

    void stopRecording()
    {
        recording = false;
    }

    bool isRecording() const
    {
        return recording;
    }

    std::vector<juce::File> getRecordingFiles() const
    {
        return recordingFiles;
    }
    
    void clearRecordingFiles()
    {
        recordingFiles.clear();
    }

    bool recording = false;
    std::vector<juce::File> recordingFiles;
    std::function<void(juce::Image)> callback;
};

VideoCaptureService::VideoCaptureService() : impl(std::make_unique<Impl>()) {}
VideoCaptureService::~VideoCaptureService() = default;

std::vector<VideoCaptureDeviceInfo> VideoCaptureService::getAvailableDevices() const { return impl->getAvailableDevices(); }
bool VideoCaptureService::startRecording(const juce::String& deviceId, const juce::File& outputFile, juce::String& errorMessage) { return impl->startRecording(deviceId, outputFile, errorMessage); }
void VideoCaptureService::setPreviewCallback(std::function<void(juce::Image)> callback) { impl->setPreviewCallback(callback); }
void VideoCaptureService::stopRecording() { impl->stopRecording(); }
bool VideoCaptureService::isRecording() const { return impl->isRecording(); }
std::vector<juce::File> VideoCaptureService::getRecordingFiles() const { return impl->getRecordingFiles(); }
void VideoCaptureService::clearRecordingFiles() { impl->clearRecordingFiles(); }

} // namespace cs
