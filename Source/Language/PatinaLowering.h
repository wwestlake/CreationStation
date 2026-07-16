#pragma once

#include <JuceHeader.h>
#include "PatinaBinder.h"
#include "PatinaIr.h"

namespace cw::patina
{
struct LoweringDiagnostic
{
    int line = 0;
    juce::String message;
};

struct LoweringResult
{
    ir::Document document;
    juce::Array<LoweringDiagnostic> diagnostics;
};

class Lowerer final
{
public:
    LoweringResult lower(const BoundSourceFile& sourceFile) const;
};
} // namespace cw::patina
