#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace cs::plugins
{
class CreationStationPluginHeader final : public juce::Component
{
public:
    explicit CreationStationPluginHeader(const juce::String& pluginName);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Image logoImage;
    juce::Label brandLabel;
    juce::Label nameLabel;
};
}
