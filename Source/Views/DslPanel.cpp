#include "DslPanel.h"

#include <CompilerApi.h>

// DslPanel's "Compile" button runs the embedded FRust compiler in this
// process (frust::Compile, CompilerApi.h): the editor's text goes in as a
// string and the diagnostics come back as data. Nothing is written to a
// file, not even a temporary one, and no other program is started. It parses
// and generates code exactly as the command-line compiler does, but never
// executes anything and never requires a `manifest "...";` declaration the
// way frust_plugin_host's load path does -- the right check for a free-form
// scratch/patch editor that isn't a loadable plugin.
DslPanel::DslPanel()
{
    setName("Code");
    headerLabel.setText("Djehuti Suite Language (FRust)", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    sourceEditor.setMultiLine(true);
    sourceEditor.setReturnKeyStartsNewLine(true);
    sourceEditor.setText(R"(// Starter FRust patch
pub fn gain(input: f64, amount: f64) -> f64 = {
    input * amount
}
)");
    addAndMakeVisible(sourceEditor);

    outputEditor.setMultiLine(true);
    outputEditor.setReadOnly(true);
    outputEditor.setText("Compile to see FRust diagnostics.");
    addAndMakeVisible(outputEditor);

    compileButton.onClick = [this] { compileSource(); };
    compileButton.setTooltip("Parse, analyze, and codegen the FRust source");
    addAndMakeVisible(compileButton);

    exportButton.onClick = [this]
    {
        if (lastCompileSucceeded && onSourceExportRequested)
            onSourceExportRequested(sourceEditor.getText(), makeSuggestedFileName());
    };
    exportButton.setTooltip("Export this source as a .frust file");
    addAndMakeVisible(exportButton);

    saveButton.onClick = [this]
    {
        if (lastCompileSucceeded && onSourceSaveToLibraryRequested)
            onSourceSaveToLibraryRequested(sourceEditor.getText(), makeSuggestedFileName());
    };
    saveButton.setTooltip("Save this source to your library");
    addAndMakeVisible(saveButton);

    loadButton.onClick = [this]
    {
        if (onSourceLoadRequested)
            onSourceLoadRequested();
    };
    loadButton.setTooltip("Load a saved .frust file");
    addAndMakeVisible(loadButton);

    outputEditor.setText("Press Compile to check this source.", juce::dontSendNotification);
    exportButton.setEnabled(false);
    saveButton.setEnabled(false);
}

void DslPanel::setSourceText(const juce::String& text)
{
    sourceEditor.setText(text, juce::dontSendNotification);
}

juce::String DslPanel::getSourceText() const
{
    return sourceEditor.getText();
}

void DslPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11151c));
}

void DslPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    headerLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);

    auto left = area.removeFromLeft(area.getWidth() / 2 - 10);
    auto right = area;
    sourceEditor.setBounds(left.withTrimmedBottom(80));
    auto buttonRow = left.removeFromBottom(72);
    compileButton.setBounds(buttonRow.removeFromLeft(160).removeFromTop(34));
    buttonRow.removeFromLeft(10);
    exportButton.setBounds(buttonRow.removeFromLeft(160).removeFromTop(34));
    buttonRow.removeFromLeft(10);
    saveButton.setBounds(buttonRow.removeFromLeft(160).removeFromTop(34));
    buttonRow.removeFromLeft(10);
    loadButton.setBounds(buttonRow.removeFromLeft(140).removeFromTop(34));
    outputEditor.setBounds(right);
}

void DslPanel::compileSource()
{
    frust::CompileRequest request;
    request.sources.push_back({ "patch.frust", sourceEditor.getText().toStdString() });
    request.emitObject = false; // check only: nothing here needs the object code

    const auto result = frust::Compile(request);
    lastCompileSucceeded = result.ok;

    juce::String output;
    if (result.ok)
    {
        output = "Compiled cleanly.";
    }
    else
    {
        for (const auto& diagnostic : result.diagnostics)
            output << juce::String(frust::FormatDiagnostic(diagnostic)) << "\n";
        if (output.isEmpty())
            output = "The FRust compiler reported a failure without a message.";
    }

    outputEditor.setText(output, juce::dontSendNotification);
    exportButton.setEnabled(lastCompileSucceeded);
    saveButton.setEnabled(lastCompileSucceeded);
}

void DslPanel::loadSourceFromFile(const juce::File& sourceFile)
{
    setSourceText(sourceFile.loadFileAsString());
    compileSource();
}

juce::String DslPanel::makeSuggestedFileName() const
{
    return "patch";
}
