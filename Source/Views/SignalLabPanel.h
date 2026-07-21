#pragma once

#include <JuceHeader.h>
#include "../Audio/PatchRuntimePlayer.h"
#include "../Patch/PatchModel.h"

class SignalLabPanel final : public juce::Component
{
public:
    struct SignalRecipe
    {
        juce::String name { "Signal-Lab-Render" };
        double sampleRate = 48000.0;
        double durationSeconds = 1.5;
        float baseFrequencyHz = 180.0f;
        float sineLevel = 0.65f;
        float sawLevel = 0.15f;
        float squareLevel = 0.08f;
        float triangleLevel = 0.12f;
        float noiseLevel = 0.10f;
        float brightness = 0.72f;
        float pitchSweepSemitones = 0.0f;
        float attackPosition = 0.12f;
        float sustainPosition = 0.42f;
        float releasePosition = 0.82f;
        float sustainLevel = 0.48f;
        std::array<float, 4> pitchAutomation { 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, 4> gainAutomation { 1.0f, 0.85f, 0.7f, 0.0f };
    };

    SignalLabPanel();

    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);
    bool loadPatchDocument(const cw::PatchDocument& document, juce::String& errorMessage);
    void applyAiTemplate(const juce::String& templateName);
    bool previewCurrentSignal();

    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onPreviewRequested;
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onRenderRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchExportRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchSaveToLibraryRequested;
    std::function<void()> onPatchLoadRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class EnvelopeEditor final : public juce::Component
    {
    public:
        EnvelopeEditor();

        void setRecipe(const SignalRecipe& recipe);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

        std::function<void(float attackPosition, float sustainPosition, float releasePosition, float sustainLevel)> onEnvelopeChanged;

    private:
        enum class DragTarget
        {
            none,
            attack,
            sustain,
            release
        };

        SignalRecipe recipe;
        DragTarget dragTarget = DragTarget::none;

        juce::Point<float> getAttackPoint() const;
        juce::Point<float> getSustainPoint() const;
        juce::Point<float> getReleasePoint() const;
        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> toScreen(float normalizedX, float normalizedY) const;
    };

    class ScopePanel final : public juce::Component
    {
    public:
        void setBuffer(const juce::AudioBuffer<float>& buffer);
        void paint(juce::Graphics& g) override;

    private:
        juce::AudioBuffer<float> displayBuffer;
    };

    class SpectrumPanel final : public juce::Component
    {
    public:
        void setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
        void paint(juce::Graphics& g) override;

    private:
        juce::Array<float> magnitudes;
    };

    class AutomationLaneEditor final : public juce::Component
    {
    public:
        AutomationLaneEditor(const juce::String& title, juce::Colour accent, float defaultMidline);

        void setValues(const std::array<float, 4>& newValues);
        std::array<float, 4> getValues() const noexcept { return values; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

        std::function<void(const std::array<float, 4>&)> onValuesChanged;

    private:
        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> getPoint(int index) const;

        juce::String laneTitle;
        juce::Colour laneAccent;
        std::array<float, 4> values;
        int dragIndex = -1;
        float defaultMidline = 0.5f;
    };

    void configureSlider(juce::Slider& slider, double min, double max, double step);
    void regenerateSignal();
    juce::AudioBuffer<float> buildSignalBuffer(const SignalRecipe& recipe) const;
    cw::PatchDocument buildPatchDocument(const SignalRecipe& recipe) const;
    void applyTemplate(const juce::String& templateName);
    void refreshControlsFromRecipe();
    void updateStatusText();

    SignalRecipe recipe;
    juce::AudioBuffer<float> generatedBuffer;
    bool suppressCallbacks = false;
    PatchRuntimePlayer runtimePlayer;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label templateLabel;
    juce::ComboBox templateSelector;
    juce::Label frequencyLabel;
    juce::Slider frequencySlider;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    juce::Label pitchLabel;
    juce::Slider pitchSlider;
    juce::Label brightnessLabel;
    juce::Slider brightnessSlider;
    juce::Label sineLabel;
    juce::Slider sineSlider;
    juce::Label sawLabel;
    juce::Slider sawSlider;
    juce::Label squareLabel;
    juce::Slider squareSlider;
    juce::Label triangleLabel;
    juce::Slider triangleSlider;
    juce::Label noiseLabel;
    juce::Slider noiseSlider;
    juce::TextButton previewButton { "Preview Signal" };
    juce::TextButton renderButton { "Render To Project" };
    juce::TextButton exportPatchButton { "Export File" };
    juce::TextButton savePatchButton { "Save Sound" };
    juce::TextButton loadPatchButton { "Load Sound" };
    EnvelopeEditor envelopeEditor;
    AutomationLaneEditor pitchAutomationEditor { "Pitch Motion", juce::Colour(0xffb37df0), 0.5f };
    AutomationLaneEditor gainAutomationEditor { "Gain Motion", juce::Colour(0xff7dd36f), 1.0f };
    ScopePanel scopePanel;
    SpectrumPanel spectrumPanel;
};
