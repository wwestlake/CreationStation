#pragma once

#include <JuceHeader.h>

namespace cs
{
// A plain image well for the Tracker's video-track scrub preview - owns no decode logic itself,
// just displays whatever juce::Image it's last handed (letterboxed to its own bounds).
class VideoPreviewComponent final : public juce::Component
{
public:
    void setImage(juce::Image newImage)
    {
        image = std::move(newImage);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xcc0a0e14));
        g.fillRect(getLocalBounds());

        if (image.isValid())
        {
            g.drawImage(image, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xff5eebd6));
            g.setFont(juce::Font(11.0f));
            g.drawText("Decoding...", getLocalBounds(), juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff5eebd6));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    juce::Image image;
};
}
