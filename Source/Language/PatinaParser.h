#pragma once

#include <JuceHeader.h>
#include "PatinaLexer.h"
#include "PatinaSurfaceAst.h"

namespace cw::patina
{
struct ParseDiagnostic
{
    int line = 0;
    int column = 0;
    juce::String message;
};

struct ParseResult
{
    SourceFile sourceFile;
    juce::Array<ParseDiagnostic> diagnostics;
};

class Parser final
{
public:
    ParseResult parse(const juce::Array<Token>& tokens) const;
};
} // namespace cw::patina
