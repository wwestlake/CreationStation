#pragma once

#include <JuceHeader.h>

// Everything the user decides before a render. Kept as plain data so the dialog, the render job and the
// "remember the last settings" logic all share one shape.
struct RenderRequest
{
    enum class Range { entireProject, custom };
    enum class Destination { project, file };
    enum class Normalize { off, peakMinus1, peakMinus03 };

    Range range = Range::entireProject;
    double customStartSeconds = 0.0;
    double customEndSeconds = 0.0;
    double tailSeconds = 2.0;         // extra time after the range so reverbs and delays can finish
    Destination destination = Destination::project;
    juce::String name;                // may contain $project and $date
    int bitsPerSample = 24;           // 16 or 24 (fixed point) or 32 (float)
    double sampleRate = 0.0;          // 0 = the audio device's rate
    Normalize normalize = Normalize::off;
    bool dither = true;               // only applies at 16-bit
    bool placeOnNewTrack = false;     // project destination only
};

// The settings dialog shown before a render (modelled on the choices a DAW's render window offers).
class RenderDialog final : public juce::Component
{
public:
    RenderDialog(const RenderRequest& initial, double projectLengthSeconds, double deviceSampleRate, bool projectHasMidiClips);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(const RenderRequest&)> onRender;
    std::function<void()> onCancel;

private:
    void updateEnabledState();
    void showMessage(const juce::String& text, bool isError);
    bool readRequest(RenderRequest& request);

    double projectLengthSeconds = 0.0;
    double deviceSampleRate = 48000.0;
    bool hasMidiClips = false;

    juce::Label sourceLabel, sourceValueLabel;
    juce::Label rangeLabel, startLabel, endLabel, tailLabel;
    juce::ComboBox rangeSelector;
    juce::TextEditor startEditor, endEditor, tailEditor;
    juce::Label destinationLabel, nameLabel;
    juce::ComboBox destinationSelector;
    juce::TextEditor nameEditor;
    juce::Label bitDepthLabel, sampleRateLabel, normalizeLabel, ditherLabel;
    juce::ComboBox bitDepthSelector, sampleRateSelector, normalizeSelector, ditherSelector;
    juce::ToggleButton placeOnTrackToggle { "Place the result on a new track" };
    juce::Label messageLabel;
    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton renderButton { "Render" };
};
