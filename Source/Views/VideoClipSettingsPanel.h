#pragma once

#include <JuceHeader.h>

#include "../Video/Gl/VideoLayerParams.h"

// The window for one video clip's effect and layout: green screen, and where the picture sits on the canvas
// (size, position, opacity) for picture-in-picture and overlays. Every change is reported straight away, so the video
// view updates while a slider moves.
class VideoClipSettingsPanel final : public juce::Component
{
public:
    explicit VideoClipSettingsPanel(const juce::NamedValueSet& initial) : values(initial)
    {
        addSection(greenScreenTitle, "Green screen");
        keyToggle.setButtonText("Remove the key colour (make it see-through)");
        keyToggle.setToggleState((bool) values.getWithDefault(cs::videoparams::keyEnabled, false), juce::dontSendNotification);
        keyToggle.onClick = [this] { set(cs::videoparams::keyEnabled, keyToggle.getToggleState()); };
        addAndMakeVisible(keyToggle);

        keyColourLabel.setText("Key colour", juce::dontSendNotification);
        addAndMakeVisible(keyColourLabel);
        keyColour.addItem("Green", 1);
        keyColour.addItem("Blue", 2);
        keyColour.setSelectedId(cs::videoparams::number(values, cs::videoparams::keyBlue, 0.0f) > 0.5f ? 2 : 1, juce::dontSendNotification);
        keyColour.onChange = [this]
        {
            const auto blue = keyColour.getSelectedId() == 2;
            values.set(cs::videoparams::keyRed, 0.0);
            values.set(cs::videoparams::keyGreen, blue ? 0.0 : 1.0);
            values.set(cs::videoparams::keyBlue, blue ? 1.0 : 0.0);
            changed();
        };
        addAndMakeVisible(keyColour);

        addSlider("Tolerance", cs::videoparams::keyTolerance, 0.0, 1.0, 0.30, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
        addSlider("Soft edge", cs::videoparams::keySoftness, 0.0, 0.5, 0.10, [](double v) { return juce::String(juce::roundToInt(v * 200.0)) + "%"; });
        addSlider("Spill cleanup", cs::videoparams::keySpill, 0.0, 1.0, 0.50, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });

        addSection(layoutTitle, "Layout (picture-in-picture, overlays)");
        addSlider("Size", cs::videoparams::layoutScale, 0.05, 2.0, 1.0, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
        addSlider("Across", cs::videoparams::layoutX, -1.0, 1.0, 0.0, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
        addSlider("Down", cs::videoparams::layoutY, -1.0, 1.0, 0.0, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
        addSlider("Opacity", cs::videoparams::layoutOpacity, 0.0, 1.0, 1.0, [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });

        hint.setText("A clip on a track higher in the list is drawn in front of one lower down.", juce::dontSendNotification);
        hint.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
        hint.setFont(juce::Font(juce::FontOptions(12.5f)));
        addAndMakeVisible(hint);

        resetButton.setButtonText("Reset everything");
        resetButton.onClick = [this] { reset(); };
        addAndMakeVisible(resetButton);

        setSize(400, 470);
    }

    // Called after every change with the clip's complete settings.
    std::function<void(const juce::NamedValueSet&)> onSettingsChanged;

    void resized() override
    {
        auto area = getLocalBounds().reduced(16, 12);
        greenScreenTitle.setBounds(area.removeFromTop(24));
        keyToggle.setBounds(area.removeFromTop(26));
        auto colourRow = area.removeFromTop(28);
        keyColourLabel.setBounds(colourRow.removeFromLeft(110));
        keyColour.setBounds(colourRow.removeFromLeft(140));
        for (auto* row : { &sliderRows[0], &sliderRows[1], &sliderRows[2] })
            placeRow(area, *row);

        area.removeFromTop(10);
        layoutTitle.setBounds(area.removeFromTop(24));
        for (auto* row : { &sliderRows[3], &sliderRows[4], &sliderRows[5], &sliderRows[6] })
            placeRow(area, *row);

        area.removeFromTop(6);
        hint.setBounds(area.removeFromTop(22));
        area.removeFromTop(6);
        resetButton.setBounds(area.removeFromTop(30).removeFromLeft(160));
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff141a24)); }

private:
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        const char* key = nullptr;
        double defaultValue = 0.0;
    };

    static void placeRow(juce::Rectangle<int>& area, Row& row)
    {
        auto line = area.removeFromTop(32);
        row.label.setBounds(line.removeFromLeft(110));
        row.slider.setBounds(line);
    }

    void addSection(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    }

    void addSlider(const juce::String& name, const char* key, double low, double high, double defaultValue, std::function<juce::String(double)> format)
    {
        auto& row = sliderRows[(size_t) nextRow++];
        row.key = key;
        row.defaultValue = defaultValue;
        row.label.setText(name, juce::dontSendNotification);
        addAndMakeVisible(row.label);

        row.slider.setSliderStyle(juce::Slider::LinearHorizontal);
        row.slider.setRange(low, high, 0.001);
        row.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
        row.slider.textFromValueFunction = std::move(format);
        row.slider.setDoubleClickReturnValue(true, defaultValue);
        row.slider.setValue((double) values.getWithDefault(key, defaultValue), juce::dontSendNotification);
        row.slider.onValueChange = [this, key, slider = &row.slider] { set(key, slider->getValue()); };
        addAndMakeVisible(row.slider);
    }

    void set(const char* key, const juce::var& value)
    {
        values.set(key, value);
        changed();
    }

    void changed()
    {
        if (onSettingsChanged)
            onSettingsChanged(values);
    }

    void reset()
    {
        values.clear();
        keyToggle.setToggleState(false, juce::dontSendNotification);
        keyColour.setSelectedId(1, juce::dontSendNotification);
        for (auto& row : sliderRows)
            if (row.key != nullptr)
                row.slider.setValue(row.defaultValue, juce::dontSendNotification);
        changed();
    }

    juce::NamedValueSet values;
    juce::Label greenScreenTitle, layoutTitle, keyColourLabel, hint;
    juce::ToggleButton keyToggle;
    juce::ComboBox keyColour;
    Row sliderRows[7];
    int nextRow = 0;
    juce::TextButton resetButton;
};
