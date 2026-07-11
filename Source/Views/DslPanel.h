#pragma once

#include <JuceHeader.h>
#include "../Language/DslCompiler.h"

class DslPanel final : public juce::Component
{
public:
    DslPanel();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void compileSource();

    juce::Label headerLabel;
    juce::TextEditor sourceEditor;
    juce::TextEditor outputEditor;
    juce::TextButton compileButton { "Compile DSP" };
    cw::DslCompiler compiler;
};
