#pragma once

#include <JuceHeader.h>
#include "../Audio/SamplePackBuilder/SamplePackBuilderEngine.h"
#include <thread>

// Dialog UI for the offline Sample Pack Builder: pick a folder of scattered single-note
// captures, pick an output folder, run the pipeline, and watch a per-file log of what got
// detected, corrected, kept, or gap-filled. Launched as a standalone dialog window (see
// PluginsPanel's "Build Sample Pack..." button), not a docked workspace tab, since this is an
// occasional batch tool rather than an always-present view.
class SamplePackBuilderPanel final : public juce::Component
{
public:
    SamplePackBuilderPanel();
    ~SamplePackBuilderPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void chooseInputFolder();
    void chooseOutputFolder();
    void startBuild();
    void appendLogLine(const juce::String& line);
    void onBuildFinished(SamplePackBuilderEngine::BuildResult result);
    void setControlsEnabled(bool enabled);

    juce::Label titleLabel;
    juce::Label inputLabel;
    juce::TextEditor inputPathEditor;
    juce::TextButton chooseInputButton { "Browse..." };
    juce::Label outputLabel;
    juce::TextEditor outputPathEditor;
    juce::TextButton chooseOutputButton { "Browse..." };
    juce::TextButton buildButton { "Build Sample Pack" };
    juce::TextEditor logEditor;

    std::unique_ptr<juce::FileChooser> activeChooser;
    std::thread buildThread;
    bool buildInProgress = false;
};
