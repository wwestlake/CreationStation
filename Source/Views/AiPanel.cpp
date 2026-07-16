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

    contextLabel.setText("Context Packet", juce::dontSendNotification);
    contextLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(contextLabel);

    contextEditor.setMultiLine(true);
    contextEditor.setReadOnly(true);
    contextEditor.setText("The local retrieval engine will assemble project context here.");
    addAndMakeVisible(contextEditor);

    responseEditor.setMultiLine(true);
    responseEditor.setReadOnly(true);
    responseEditor.setText("The assistant will draft DSP ideas, node graphs, or DSL code here.");
    addAndMakeVisible(responseEditor);

    generateButton.onClick = [this]
    {
        if (onPromptSubmitted)
            onPromptSubmitted(promptEditor.getText());
        else
            responseEditor.setText("AI request queued for: " + promptEditor.getText(),
                                   juce::dontSendNotification);
    };
    addAndMakeVisible(generateButton);
}

void AiPanel::setContextPacket(const CreationStationContextEngine::ContextPacket& packet)
{
    contextEditor.setText(packet.summary, juce::dontSendNotification);

    juce::String response;
    response << "Context-aware drafting basis:\n\n";
    for (const auto& snippet : packet.snippets)
        response << "- " << snippet.title << ": " << snippet.excerpt << "\n";

    if (packet.snippets.isEmpty())
        response << "No strong local context matched yet. The assistant should ask for more specifics or fall back to general design help.";

    responseEditor.setText(response, juce::dontSendNotification);
}

juce::String AiPanel::getPromptText() const
{
    return promptEditor.getText();
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

    auto promptArea = area.removeFromTop(120);
    promptEditor.setBounds(promptArea.withTrimmedBottom(36));
    generateButton.setBounds(promptArea.removeFromBottom(30).removeFromRight(160));

    auto middle = area.removeFromTop(area.getHeight() / 2 - 10);
    contextLabel.setBounds(middle.removeFromTop(22));
    contextEditor.setBounds(middle);
    area.removeFromTop(10);
    responseEditor.setBounds(area);
}
