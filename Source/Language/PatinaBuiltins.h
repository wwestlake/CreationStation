#pragma once

#include <JuceHeader.h>
#include "PatinaIr.h"
#include "PatinaSurfaceAst.h"

namespace cw::patina
{
struct BuiltinArgumentSignature
{
    juce::String name;
    juce::String valueType;
    bool required = false;
};

struct BuiltinPortSignature
{
    juce::String name;
    juce::String valueType;
};

struct BuiltinNodeSignature
{
    juce::String qualifiedName;
    ir::Domain domain = ir::Domain::worker;
    juce::Array<BuiltinArgumentSignature> arguments;
    juce::Array<BuiltinPortSignature> inputs;
    juce::Array<BuiltinPortSignature> outputs;
};

class BuiltinsRegistry final
{
public:
    const BuiltinNodeSignature* findNode(const QualifiedName& callee) const;
    const BuiltinArgumentSignature* findArgument(const BuiltinNodeSignature& signature, const juce::String& name) const;
    const BuiltinPortSignature* findInput(const BuiltinNodeSignature& signature, const juce::String& name) const;
    const BuiltinPortSignature* findOutput(const BuiltinNodeSignature& signature, const juce::String& name) const;
    bool isValueAssignableTo(const juce::String& sourceType, const juce::String& destinationType) const;

private:
    const juce::Array<BuiltinNodeSignature>& getNodes() const;
};
} // namespace cw::patina
