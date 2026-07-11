#include "DslPanel.h"

DslPanel::DslPanel()
{
    headerLabel.setText("Functional DSP DSL", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    sourceEditor.setMultiLine(true);
    sourceEditor.setReturnKeyStartsNewLine(true);
    sourceEditor.setText(R"(# Creative Workstation DSP script
let drive = saturate(0.35)
let space = reverb(room = 0.62, mix = 0.18)
route master = input |> drive |> space |> limiter()
)");
    addAndMakeVisible(sourceEditor);

    outputEditor.setMultiLine(true);
    outputEditor.setReadOnly(true);
    outputEditor.setText("Compile to see diagnostics.");
    addAndMakeVisible(outputEditor);

    compileButton.onClick = [this] { compileSource(); };
    addAndMakeVisible(compileButton);
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
    compileButton.setBounds(left.removeFromBottom(34).removeFromRight(140));
    outputEditor.setBounds(right);
}

void DslPanel::compileSource()
{
    auto result = compiler.compile(sourceEditor.getText());
    juce::String output;

    if (result.success)
    {
        output << "OK\n" << result.summary;
    }
    else
    {
        for (const auto& diagnostic : result.diagnostics)
            output << "Line " << diagnostic.line << ": " << diagnostic.message << "\n";
    }

    outputEditor.setText(output, juce::dontSendNotification);
}
