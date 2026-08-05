#pragma once

#include <JuceHeader.h>
#include "../Audio/PatchRuntimePlayer.h"
#include "../Patch/PatchModel.h"

class SignalLabPanel final : public juce::Component
{
public:
    struct SignalRecipe
    {
        SignalRecipe();

        juce::String name { "Signal-Lab-Render" };
        double sampleRate = 48000.0;
        double durationSeconds = 1.5;
        float baseFrequencyHz = 180.0f;
        float sineLevel = 0.65f;
        float sawLevel = 0.15f;
        float squareLevel = 0.08f;
        float triangleLevel = 0.12f;
        float noiseLevel = 0.10f;
        juce::String filterMode { "lowpass" };
        float filterCutoffHz = 3600.0f;
        float filterResonance = 0.90f;
        float filterEnvelopeAmount = 0.35f;
        float macroHardness = 0.50f;
        float macroWeight = 0.50f;
        float macroAir = 0.50f;
        float macroGrit = 0.25f;
        float macroSize = 0.50f;
        juce::String envelopeCurveMode { "smooth" };
        juce::String automationCurveMode { "smooth" };
        float pitchSweepSemitones = 0.0f;
        juce::Array<cw::PatchAutomationPoint> envelopePoints;
        juce::Array<cw::PatchAutomationLane> automationLanes;
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
        void mouseDoubleClick(const juce::MouseEvent& event) override;

        std::function<void(const juce::Array<cw::PatchAutomationPoint>&)> onEnvelopeChanged;

    private:
        SignalRecipe recipe;
        int dragIndex = -1;

        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> toScreen(float normalizedX, float normalizedY) const;
        juce::Point<float> getPoint(int index) const;
        int findPointAt(juce::Point<float> position) const;
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
        AutomationLaneEditor() = default;

        void setLane(const cw::PatchAutomationLane& newLane, juce::Colour accentColour);
        const cw::PatchAutomationLane& getLane() const noexcept { return lane; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;

        std::function<void(const cw::PatchAutomationLane&)> onLaneChanged;

    private:
        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> getPoint(int index) const;
        int findPointAt(juce::Point<float> position) const;
        double pointValueFromY(float y) const;

        cw::PatchAutomationLane lane;
        juce::Colour laneAccent;
        int dragIndex = -1;
    };

    void configureSlider(juce::Slider& slider, double min, double max, double step);
    void regenerateSignal();
    juce::AudioBuffer<float> buildSignalBuffer(const SignalRecipe& recipe) const;
    cw::PatchDocument buildPatchDocument(const SignalRecipe& recipe) const;
    void applyTemplate(const juce::String& templateName);
    void refreshControlsFromRecipe();
    void updateStatusText();
    void syncAutomationEditors();
    void rebuildAutomationChrome();

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
    juce::Label filterModeLabel;
    juce::ComboBox filterModeSelector;
    juce::Label filterCutoffLabel;
    juce::Slider filterCutoffSlider;
    juce::Label filterResonanceLabel;
    juce::Slider filterResonanceSlider;
    juce::Label filterEnvelopeLabel;
    juce::Slider filterEnvelopeSlider;
    juce::Label envelopeCurveLabel;
    juce::ComboBox envelopeCurveSelector;
    juce::Label automationCurveLabel;
    juce::ComboBox automationCurveSelector;
    juce::Label macroHardnessLabel;
    juce::Slider macroHardnessSlider;
    juce::Label macroWeightLabel;
    juce::Slider macroWeightSlider;
    juce::Label macroAirLabel;
    juce::Slider macroAirSlider;
    juce::Label macroGritLabel;
    juce::Slider macroGritSlider;
    juce::Label macroSizeLabel;
    juce::Slider macroSizeSlider;
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
    juce::TextButton addAutomationLaneButton { "Add Motion Lane" };
    EnvelopeEditor envelopeEditor;
    juce::OwnedArray<AutomationLaneEditor> automationLaneEditors;
    juce::OwnedArray<juce::ComboBox> automationTargetSelectors;
    juce::OwnedArray<juce::TextButton> removeAutomationLaneButtons;
    ScopePanel scopePanel;
    SpectrumPanel spectrumPanel;
};
