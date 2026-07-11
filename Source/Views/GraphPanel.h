#pragma once

#include <JuceHeader.h>

class GraphPanel final : public juce::Component
{
public:
    GraphPanel();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label headerLabel;
    juce::StringArray nodeNames;
};
