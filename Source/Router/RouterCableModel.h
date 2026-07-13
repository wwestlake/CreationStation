#pragma once

#include <JuceHeader.h>

class RouterCableModel final
{
public:
    enum class CableRole
    {
        monitor,
        capture,
        mic,
        instrument
    };

    struct CableDescriptor
    {
        CableRole role = CableRole::monitor;
        juce::String name;
        juce::String purpose;
        bool enabled = false;
    };

    RouterCableModel();

    const juce::Array<CableDescriptor>& getCables() const noexcept { return cables; }
    void setSourceIndex(int newIndex) noexcept { sourceIndex = juce::jmax(0, newIndex); }
    int getSourceIndex() const noexcept { return sourceIndex; }

    void setCableEnabled(int index, bool shouldEnable);
    bool isCableEnabled(int index) const;
    juce::String getSourceName(int index) const;
    juce::String buildSummary() const;

    juce::StringArray getCableNames() const;
    juce::StringArray getCablePurposes() const;

private:
    juce::Array<CableDescriptor> cables;
    int sourceIndex = 0;
};
