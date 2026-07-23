#include "CreationStationPluginLookAndFeel.h"
#include "CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationPluginLookAndFeel::CreationStationPluginLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, palette::textSecondary);
}

void CreationStationPluginLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                        float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                                        juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(6.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(palette::knobTrack);
    g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(slider.isEnabled() ? palette::accent : palette::textSecondary.withAlpha(0.4f));
    g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobRadius = radius * 0.68f;
    auto knobBounds = juce::Rectangle<float>(knobRadius * 2.0f, knobRadius * 2.0f).withCentre(centre);
    g.setColour(palette::panelBackground);
    g.fillEllipse(knobBounds);
    g.setColour(palette::outline);
    g.drawEllipse(knobBounds, 1.5f);

    juce::Path pointer;
    auto pointerLength = knobRadius * 0.82f;
    auto pointerThickness = 3.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.6f, pointerThickness * 0.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
    g.setColour(palette::textPrimary);
    g.fillPath(pointer);
}

void CreationStationPluginLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto isOn = button.getToggleState();

    g.setColour(isOn ? palette::accent.withAlpha(0.85f) : palette::panelBackground);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? palette::accent : palette::outline);
    g.drawRoundedRectangle(bounds, 6.0f, 1.2f);
}

juce::Font CreationStationPluginLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(13.0f);
}

}
