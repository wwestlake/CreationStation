#pragma once

#include <JuceHeader.h>
#include "../Language/DslCompiler.h"
#include "../Language/PatinaArtifactLoader.h"

class DslPanel final : public juce::Component
{
public:
    DslPanel();

    void setSourceText(const juce::String& text);
    juce::String getSourceText() const;
    void showLoadedArtifactSummary(const cw::patina::ir::Document& document, const juce::File& sourceFile);

    std::function<void(const juce::String& artifactJson, const juce::String& suggestedName)> onArtifactExportRequested;
    std::function<void(const juce::String& artifactJson, const juce::String& suggestedName)> onArtifactSaveToLibraryRequested;
    std::function<void()> onArtifactLoadRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void compileSource();
    juce::String makeSuggestedArtifactName() const;

    juce::Label headerLabel;
    juce::TextEditor sourceEditor;
    juce::TextEditor outputEditor;
    juce::TextButton compileButton { "Compile Patina" };
    juce::TextButton exportButton { "Export Artifact" };
    juce::TextButton saveButton { "Save To Library" };
    juce::TextButton loadButton { "Load Artifact" };
    cw::DslCompiler compiler;
    cw::DslModule lastModule;
};
