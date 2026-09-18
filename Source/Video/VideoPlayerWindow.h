#pragma once
#include <JuceHeader.h>
#include "VideoPreviewComponent.h"

namespace cs {

class VideoPlayerWindow : public juce::DocumentWindow {
public:
    VideoPlayerWindow(const juce::String& name, juce::Colour backgroundColour)
        : juce::DocumentWindow(name, backgroundColour, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(&previewComponent, true);
        setResizable(true, false);
        setResizeLimits(320, 180, 3840, 2160);
        centreWithSize(640, 360);
    }
    
    void closeButtonPressed() override {
        setVisible(false);
    }
    
    VideoPreviewComponent& getPreviewComponent() {
        return previewComponent;
    }

private:
    VideoPreviewComponent previewComponent;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoPlayerWindow)
};

}