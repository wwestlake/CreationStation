#pragma once

#include <JuceHeader.h>
#include "../Patch/PatchModel.h"

class PatchRuntimePlayer final
{
public:
    // A Scope/Analyzer node's actual signal, sampled at whatever point in
    // the chain it's wired to (a specific source, the mix bus, post-envelope,
    // or post-filter) -- not just a copy of the final output, so the node
    // shows what's really flowing past it.
    struct TapCapture
    {
        juce::String nodeId;
        juce::AudioBuffer<float> buffer;
    };

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();

    bool renderPatchToBuffer(const cw::PatchDocument& patch,
                             double durationSeconds,
                             juce::AudioBuffer<float>& destination,
                             juce::String& errorMessage,
                             juce::Array<TapCapture>* taps = nullptr) const;

private:
    static const cw::PatchNode* findNode(const cw::PatchDocument& patch, const juce::String& kind);

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
};
