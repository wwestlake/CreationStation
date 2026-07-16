#pragma once

#include <JuceHeader.h>
#include "PatinaBinder.h"
#include "PatinaIr.h"
#include "PatinaLowering.h"
#include "PatinaSurfaceAst.h"

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
    int graphCount = 0;
    int importCount = 0;
    int exportCount = 0;
    int parameterCount = 0;
    int letCount = 0;
    juce::String packageName;
    juce::String version;
    juce::String debugTree;
    patina::SourceFile surfaceAst;
    patina::BoundSourceFile boundAst;
    patina::ir::Document semanticIr;
};

class DslCompiler final
{
public:
    DslModule compile(const juce::String& source) const;
};
} // namespace cw
