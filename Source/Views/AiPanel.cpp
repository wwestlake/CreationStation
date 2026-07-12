#include "AiPanel.h"

AiPanel::AiPanel()
{
    setName("AI");
    headerLabel.setText("AI Assist", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setText("Describe the sound or transformation you want.");
    addAndMakeVisible(promptEditor);

    responseEditor.setMultiLine(true);
    responseEditor.setReadOnly(true);
    responseEditor.setText("The assistant will draft DSP ideas, node graphs, or DSL code here.");
    addAndMakeVisible(responseEditor);

    generateButton.onClick = [this]
    {
        responseEditor.setText("Drafting a graph and DSL sketch for: " + promptEditor.getText(),
                               juce::dontSendNotification);
    };
    addAndMakeVisible(generateButton);
}

void AiPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141820));
}

void AiPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    headerLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);

    auto top = area.removeFromTop(area.getHeight() / 2 - 8);
    promptEditor.setBounds(top.withTrimmedBottom(36));
    generateButton.setBounds(top.removeFromBottom(30).removeFromRight(160));
    responseEditor.setBounds(area);
}
