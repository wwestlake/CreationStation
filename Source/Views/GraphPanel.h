#pragma once

#include <JuceHeader.h>
#include "../Language/SignalGraph.h"

class GraphPanel final : public juce::Component
{
public:
    GraphPanel();

    void setDrive(float amount);
    void setInput(float amount);
    void setTone(float amount);
    void setEcho(float amount);
    void setWidth(float amount);
    void setEnabled(bool shouldEnable);

    std::function<void(float)> onDriveChanged;
    std::function<void(float)> onInputChanged;
    std::function<void(float)> onToneChanged;
    std::function<void(float)> onEchoChanged;
    std::function<void(float)> onWidthChanged;
    std::function<void(bool)> onEnabledChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct NodeCard final : public juce::Component
    {
        NodeCard(const cw::NodeTemplate& nodeTemplate);

        void setAmount(float newAmount);
        void setEnabled(bool shouldEnable);

        void paint(juce::Graphics&) override;
        void resized() override;

        juce::Label title;
        juce::Label subtitle;
        juce::Label categoryLabel;
        juce::Slider amountSlider;
        juce::ToggleButton enableToggle { "On" };
        cw::NodeTemplate nodeTemplate;
        float amount = 0.5f;
        bool enabled = true;
    };

    juce::Label headerLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton graphEnabledToggle { "Graph On" };
    juce::OwnedArray<NodeCard> nodeCards;
};
