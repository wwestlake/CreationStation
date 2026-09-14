#include "DslPanel.h"

// DslPanel's "Compile" button shells out to the real frust_compiler CLI in
// --emit-obj mode (parses, runs sema/codegen, emits an object file, never
// executes anything) rather than linking an embeddable FRust frontend
// library -- FrustLang doesn't package its parser/sema as one today (see
// CS_FRUST_COMPILER_EXECUTABLE's own comment in CMakeLists.txt). This also
// means a bare, manifest-free patch compiles here exactly like it did under
// CEL -- frust_plugin_host's JIT load path (used by Signal Lab/Foley's real
// node-graph output) refuses to load anything without a `manifest "...";`
// declaration, which would be unwanted boilerplate for a scratch patch.
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

    compileSource();
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
    const auto scratchDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("djehuti_station_dsl_panel");
    scratchDir.createDirectory();
    const auto sourceFile = scratchDir.getChildFile("patch.frust");
    const auto objectFile = scratchDir.getChildFile("patch.o");
    sourceFile.replaceWithText(sourceEditor.getText());

    juce::ChildProcess compilerProcess;
    const juce::StringArray arguments {
        CS_FRUST_COMPILER_EXECUTABLE,
        "--emit-obj",
        objectFile.getFullPathName(),
        sourceFile.getFullPathName()
    };

    juce::String output;
    if (!compilerProcess.start(arguments, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        output = "Could not launch frust_compiler.";
        lastCompileSucceeded = false;
    }
    else
    {
        output = compilerProcess.readAllProcessOutput();
        compilerProcess.waitForProcessToFinish(10000);

        // frust_compiler prints nothing at all on a clean --emit-obj compile
        // (see its own Main.cpp) -- any output at all means a parse/sema/
        // codegen diagnostic fired. Not the process exit code: a known,
        // separately-tracked frust_compiler bug means it doesn't yet exit
        // nonzero on a compile error, so exit code can't be trusted here.
        lastCompileSucceeded = output.isEmpty();
    }

    if (lastCompileSucceeded)
        output = "Compiled cleanly.";

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
