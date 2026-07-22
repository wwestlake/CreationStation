#pragma once

#include <JuceHeader.h>
#include <vector>

namespace cs
{
struct HardwareInputSource
{
    int channelIndex = -1;
    juce::String id;
    juce::String name;
};

struct StudioInputSource
{
    juce::String id;
    juce::String name;
    int channelIndex = -1;
    juce::String hardwareName;
    bool available = false;
};

class StudioIOModel final
{
public:
    void setHardwareInputs(const juce::Array<HardwareInputSource>& hardwareInputs);
    const std::vector<StudioInputSource>& getInputs() const noexcept { return inputs; }

    juce::Array<juce::String> getDisplayNames() const;
    juce::StringArray getNames() const;
    juce::StringArray getHardwareNames() const;
    juce::Array<bool> getAvailability() const;
    int getChannelForInputIndex(int inputIndex) const noexcept;
    int getInputIndexForChannel(int channelIndex) const noexcept;
    void setInputName(int inputIndex, const juce::String& name);

    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);

private:
    std::vector<StudioInputSource> inputs;

    static juce::String makeDefaultName(int channelIndex);
    void ensureInputForHardware(const HardwareInputSource& hardwareInput);
};
}
