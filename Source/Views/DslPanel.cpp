#include "DslPanel.h"

DslPanel::DslPanel()
{
    setName("DSL");
    headerLabel.setText("Patina Surface Language", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    sourceEditor.setMultiLine(true);
    sourceEditor.setReturnKeyStartsNewLine(true);
    sourceEditor.setText(R"(# Patina starter patch
package "@lagdaemon/soft-keys"
version "0.1.0"

import "@lagdaemon/core-audio"

graph main:
    param cutoff: control<f32> = 1200.0
    let base_hz = 220.0
    node midi   = event.midi_input()
    node notehz = event.note_to_frequency()
    node osc    = audio.oscillator(waveform: "triangle", frequency: base_hz)
    node env    = control.envelope.adsr(attack: 0.05, decay: 0.12, sustain: 0.72, release: 0.28)
    node amp    = audio.gain()
    node out    = audio.output()

    connect midi.note -> notehz.note
    connect notehz.frequency -> osc.frequency
    connect osc.out -> amp.in
    connect env.out -> amp.gain
    connect amp.out -> out.in

export instrument main
)");
    addAndMakeVisible(sourceEditor);

    outputEditor.setMultiLine(true);
    outputEditor.setReadOnly(true);
    outputEditor.setText("Compile to see Patina AST and graph diagnostics.");
    addAndMakeVisible(outputEditor);

    compileButton.onClick = [this] { compileSource(); };
    addAndMakeVisible(compileButton);

    exportButton.onClick = [this]
    {
        if (lastModule.success && onArtifactExportRequested)
            onArtifactExportRequested(lastModule.artifactJson, makeSuggestedArtifactName());
    };
    addAndMakeVisible(exportButton);

    saveButton.onClick = [this]
    {
        if (lastModule.success && onArtifactSaveToLibraryRequested)
            onArtifactSaveToLibraryRequested(lastModule.artifactJson, makeSuggestedArtifactName());
    };
    addAndMakeVisible(saveButton);

    loadButton.onClick = [this]
    {
        if (onArtifactLoadRequested)
            onArtifactLoadRequested();
    };
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
    auto result = compiler.compile(sourceEditor.getText());
    lastModule = result;
    juce::String output;

    if (result.success)
    {
        output << result.summary;
    }
    else
    {
        for (const auto& diagnostic : result.diagnostics)
            output << "Line " << diagnostic.line << ": " << diagnostic.message << "\n";
    }

    outputEditor.setText(output, juce::dontSendNotification);
    exportButton.setEnabled(result.success);
    saveButton.setEnabled(result.success);
}

juce::String DslPanel::makeSuggestedArtifactName() const
{
    auto packageName = lastModule.packageName;
    if (packageName.isEmpty())
        return "patina-artifact";

    auto suggested = packageName.toLowerCase();
    suggested = suggested.replace("@", "");
    suggested = suggested.replace("/", "-");
    suggested = suggested.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_");
    return suggested.isNotEmpty() ? suggested : "patina-artifact";
}

void DslPanel::showLoadedArtifactSummary(const cw::patina::ir::Document& document, const juce::File& sourceFile)
{
    juce::StringArray lines;
    lines.add("Loaded Patina artifact");
    lines.add("File: " + sourceFile.getFullPathName());
    lines.add("Package: " + document.packageName);
    lines.add("Version: " + document.packageVersion);
    lines.add("Schema: " + document.schemaVersion + "  Runtime: " + document.runtimeVersion + "  ABI: " + document.abiVersion);
    lines.add("Graphs: " + juce::String(document.graphs.size()) + "  |  Exports: " + juce::String(document.exports.size()));
    lines.add("");

    for (const auto& graph : document.graphs)
    {
        lines.add("graph " + graph.name);
        for (const auto& parameter : graph.parameters)
            lines.add("  param " + parameter.name + " : " + parameter.type.toString());
        for (const auto& node : graph.nodes)
            lines.add("  node " + node.id + " :: " + node.kind);
        for (const auto& edge : graph.edges)
            lines.add("  edge " + edge.sourceNode + "." + edge.sourcePort
                      + " -> " + edge.destinationNode + "." + edge.destinationPort
                      + " : " + edge.signalType.toString());
        lines.add("");
    }

    outputEditor.setText(lines.joinIntoString("\n"), juce::dontSendNotification);
}
