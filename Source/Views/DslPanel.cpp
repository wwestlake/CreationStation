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
    sourceEditor.setBounds(left.withTrimmedBottom(40));
    compileButton.setBounds(left.removeFromBottom(34).removeFromRight(160));
    outputEditor.setBounds(right);
}

void DslPanel::compileSource()
{
    auto result = compiler.compile(sourceEditor.getText());
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
}
