#pragma once

#include <JuceHeader.h>
#include "PatinaIr.h"

namespace cw::patina
{
class ArtifactLoader final
{
public:
    bool loadJson(const juce::String& jsonText, ir::Document& document, juce::String& errorMessage) const;
};
} // namespace cw::patina
