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
        receivedFrame = true;
        idle = false;
        repaint();
    }

    void resetDecodingState()
    {
        receivedFrame = false;
        idle = false;
        image = {};
        repaint();
    }

    // Nothing to show (the playhead is not over a video clip): a plain dark well, no "Decoding..." text.
    void setIdle()
    {
        if (idle)
            return;
        idle = true;
        image = {};
        repaint();
    }

    // Called when the view is resized, so the owner can ask for frames at the new size.
    std::function<void()> onSizeChanged;
    void resized() override
    {
        if (onSizeChanged)
            onSizeChanged();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xcc0a0e14));
        g.fillRect(getLocalBounds());

        if (idle)
        {
            g.setColour(juce::Colour(0xff5b6678));
            g.setFont(juce::Font(juce::FontOptions(13.0f)));
            g.drawText("No video at the playhead", getLocalBounds(), juce::Justification::centred);
        }
        else if (image.isValid())
        {
            g.drawImage(image, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
        else if (!receivedFrame)
        {
            g.setColour(juce::Colour(0xff5eebd6));
            g.setFont(juce::Font(11.0f));
            g.drawText("Decoding...", getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xffff6666));
            g.setFont(juce::Font(11.0f));
            g.drawText("Decode Failed", getLocalBounds(), juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff5eebd6));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    juce::Image image;
    bool receivedFrame = false;
    bool idle = true;
};
}
