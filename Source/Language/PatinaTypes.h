#pragma once

#include <JuceHeader.h>

namespace cw::patina
{
struct TypeSpec
{
    juce::String family;
    juce::StringArray genericArguments;
    juce::String rawText;

    bool isValid() const noexcept { return family.isNotEmpty(); }
    juce::String toString() const;
};

class TypeSystem final
{
public:
    TypeSpec parseType(const juce::String& text) const;
    bool isAssignableTo(const TypeSpec& source, const TypeSpec& destination) const;
};
} // namespace cw::patina
