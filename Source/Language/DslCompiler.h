#pragma once

#include <JuceHeader.h>

namespace cw
{
struct DslDiagnostic
{
    int line = 0;
    juce::String message;
};

struct DslModule
{
    bool success = false;
    juce::String summary;
    juce::Array<DslDiagnostic> diagnostics;
    int sourceCount = 0;
    int effectCount = 0;
    int sinkCount = 0;
    int connectionCount = 0;
    int modulationCount = 0;
};

class DslCompiler final
{
public:
    DslModule compile(const juce::String& source) const;

private:
    static bool isRecognisedStatement(const juce::String& line);
};
} // namespace cw
