#pragma once

#include <JuceHeader.h>
#include "../Patch/PatchModel.h"

class PatchRuntimePlayer final
{
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();

    bool renderPatchToBuffer(const cw::PatchDocument& patch,
                             double durationSeconds,
                             juce::AudioBuffer<float>& destination,
                             juce::String& errorMessage) const;

private:
    static float sampleAutomation(const cw::PatchAutomationLane* lane, float t);
    static const cw::PatchAutomationLane* findLane(const cw::PatchDocument& patch, const juce::String& targetParameter);
    static const cw::PatchNode* findNode(const cw::PatchDocument& patch, const juce::String& kind);

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
};
