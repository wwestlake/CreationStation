#pragma once

#include <JuceHeader.h>
#include "CreationStationTuneProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include <array>

namespace cs::plugins
{
class CreationStationTuneEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit CreationStationTuneEditor(CreationStationTuneProcessor& processorToEdit);
    ~CreationStationTuneEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PitchDisplay final : public juce::Component
    {
    public:
        void setDetectedNote(const juce::String& note) { detectedNote = note; repaint(); }
        void setTargetNote(const juce::String& note) { targetNote = note; repaint(); }
        void setDetectedCents(float cents) { detectedCents = cents; repaint(); }
        void setAppliedCents(float cents) { appliedCents = cents; repaint(); }
        void setConfidence(float confidenceValue) { confidence = confidenceValue; repaint(); }
        void setHistory(const std::array<float, CreationStationTuneProcessor::displayHistorySize>& values, int count);
        void paint(juce::Graphics& g) override;

    private:
        juce::String detectedNote { "A4" };
        juce::String targetNote { "A4" };
        float detectedCents = 0.0f;
        float appliedCents = 0.0f;
        float confidence = 0.0f;
        std::array<float, CreationStationTuneProcessor::displayHistorySize> history {};
        int historyCount = 0;
    };

    struct KnobControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText);
    void updateManualVisibility();

    CreationStationTuneProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Tune" };

    juce::Label modeLabel;
    juce::ComboBox modeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    PitchDisplay pitchDisplay;
    juce::Label keyLabel;
    juce::ComboBox keyCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    juce::Label scaleLabel;
    juce::ComboBox scaleCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    KnobControl referenceKnob;
    KnobControl speedKnob;
    KnobControl strengthKnob;
    KnobControl mixKnob;

    juce::ToggleButton octaveProtectButton { "Octave Protect" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> octaveProtectAttachment;

    std::array<juce::ToggleButton, 12> noteButtons {{
        juce::ToggleButton("C"), juce::ToggleButton("C#"), juce::ToggleButton("D"), juce::ToggleButton("D#"),
        juce::ToggleButton("E"), juce::ToggleButton("F"), juce::ToggleButton("F#"), juce::ToggleButton("G"),
        juce::ToggleButton("G#"), juce::ToggleButton("A"), juce::ToggleButton("A#"), juce::ToggleButton("B")
    }};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> noteAttachments;
    juce::Label notesLabel;

    juce::Label helpLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationTuneEditor)
};
}
