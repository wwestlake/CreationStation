#pragma once

#include <JuceHeader.h>
#include "PatinaIr.h"

namespace cw::patina
{
class ArtifactWriter final
{
public:
    juce::String writeJson(const ir::Document& document) const;
};
} // namespace cw::patina
