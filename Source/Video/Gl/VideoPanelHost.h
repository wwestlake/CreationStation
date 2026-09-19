#pragma once

#include <JuceHeader.h>

#include "VideoGlView.h"

namespace cs
{
// The video panel's content: the GL picture with a one-line status strip under it. The strip says what the graphics
// side is doing (context up, shaders compiled, layers, frames drawn), so when the picture does not appear the panel
// itself shows why instead of just staying blank.
class VideoPanelHost final : public juce::Component, private juce::Timer
{
public:
    explicit VideoPanelHost(VideoGlView& viewToHost) : view(viewToHost)
    {
        addAndMakeVisible(view);

        status.setFont(juce::Font(juce::FontOptions(11.5f)));
        status.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
        status.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0d1118));
        status.setJustificationType(juce::Justification::centredLeft);
        status.setMinimumHorizontalScale(0.7f);
        addAndMakeVisible(status);

        startTimerHz(4);
    }

    ~VideoPanelHost() override { stopTimer(); }

    // What the app side is doing, shown next to the graphics side's state (set once per playhead tick).
    void setExtraInfo(const juce::String& text) { extraInfo = text; }

    void resized() override
    {
        auto area = getLocalBounds();
        status.setBounds(area.removeFromBottom(20));
        view.setBounds(area);
    }

private:
    void timerCallback() override
    {
        status.setText(" " + view.describeState() + "  |  on screen " + (view.isShowing() ? "yes" : "NO") + "  |  " + extraInfo, juce::dontSendNotification);
    }

    VideoGlView& view;
    juce::Label status;
    juce::String extraInfo { "waiting for the first playhead tick" };
};
}
