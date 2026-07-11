#pragma once

#include <JuceHeader.h>

class AiPanel final : public juce::Component
{
public:
    AiPanel();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label headerLabel;
    juce::TextEditor promptEditor;
    juce::TextEditor responseEditor;
    juce::TextButton generateButton { "Ask AI" };
};
