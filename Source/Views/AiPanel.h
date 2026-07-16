#pragma once

#include <JuceHeader.h>
#include "../AI/CreationStationContextEngine.h"

class AiPanel final : public juce::Component
{
public:
    AiPanel();

    void setContextPacket(const CreationStationContextEngine::ContextPacket& packet);
    juce::String getPromptText() const;

    std::function<void(const juce::String& prompt)> onPromptSubmitted;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label headerLabel;
    juce::Label contextLabel;
    juce::TextEditor promptEditor;
    juce::TextEditor contextEditor;
    juce::TextEditor responseEditor;
    juce::TextButton generateButton { "Ask AI" };
};
