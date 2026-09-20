#include "DslPanel.h"

// The Script panel does not compile anything itself: pressing Compile hands the text to the host
// (onCompileRequested), which stores it in the project VFS and compiles it there with the FRust
// libraries built into the app. The panel only shows what comes back.
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
    CompileOutcome outcome;
    if (onCompileRequested)
        outcome = onCompileRequested(sourceEditor.getText());
    else
        outcome.output = "Compiling is not available here.";

    lastCompileSucceeded = outcome.ok;
    outputEditor.setText(outcome.output, juce::dontSendNotification);
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
