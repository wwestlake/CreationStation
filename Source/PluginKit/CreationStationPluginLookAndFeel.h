#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace cs::plugins
{
class CreationStationPluginLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CreationStationPluginLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label& label) override;
};
}
