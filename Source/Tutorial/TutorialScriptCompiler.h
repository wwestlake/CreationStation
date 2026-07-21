#pragma once

#include <JuceHeader.h>
#include "GuidedTutorial.h"

namespace cw::tutorial
{
class ScriptCompiler
{
public:
    bool compile(const juce::String& source, Script& script, juce::String& errorMessage) const;

private:
    static juce::StringArray tokenizeLine(const juce::String& line);
    static juce::String stripQuotes(const juce::String& text);
    static bool parseBool(const juce::String& text, bool& value);
    static juce::String makeDefaultId(const juce::String& text);
    static bool parseActionType(const juce::String& text, ActionType& type);
};
}
