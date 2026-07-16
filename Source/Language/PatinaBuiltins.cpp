#include "PatinaBuiltins.h"

namespace cw::patina
{
namespace
{
BuiltinNodeSignature makeNode(const juce::String& qualifiedName,
                              ir::Domain domain,
                              std::initializer_list<BuiltinArgumentSignature> arguments,
                              std::initializer_list<BuiltinPortSignature> inputs,
                              std::initializer_list<BuiltinPortSignature> outputs)
{
    BuiltinNodeSignature node;
    node.qualifiedName = qualifiedName;
    node.domain = domain;

    for (const auto& argument : arguments)
        node.arguments.add(argument);

    for (const auto& input : inputs)
        node.inputs.add(input);

    for (const auto& output : outputs)
        node.outputs.add(output);

    return node;
}
} // namespace

const BuiltinNodeSignature* BuiltinsRegistry::findNode(const QualifiedName& callee) const
{
    auto qualifiedName = callee.toString();
    for (const auto& node : getNodes())
        if (node.qualifiedName == qualifiedName)
            return &node;

    return nullptr;
}

const BuiltinArgumentSignature* BuiltinsRegistry::findArgument(const BuiltinNodeSignature& signature, const juce::String& name) const
{
    for (const auto& argument : signature.arguments)
        if (argument.name == name)
            return &argument;

    return nullptr;
}

const BuiltinPortSignature* BuiltinsRegistry::findInput(const BuiltinNodeSignature& signature, const juce::String& name) const
{
    for (const auto& input : signature.inputs)
        if (input.name == name)
            return &input;

    return nullptr;
}

const BuiltinPortSignature* BuiltinsRegistry::findOutput(const BuiltinNodeSignature& signature, const juce::String& name) const
{
    for (const auto& output : signature.outputs)
        if (output.name == name)
            return &output;

    return nullptr;
}

bool BuiltinsRegistry::isValueAssignableTo(const juce::String& sourceType, const juce::String& destinationType) const
{
    if (sourceType == destinationType)
        return true;

    if (sourceType == "i64" && destinationType == "f32")
        return true;

    if ((sourceType == "f32" || sourceType == "i64") && destinationType == "control<f32>")
        return true;

    if (sourceType == "bool" && destinationType == "control<bool>")
        return true;

    return false;
}

const juce::Array<BuiltinNodeSignature>& BuiltinsRegistry::getNodes() const
{
    static const juce::Array<BuiltinNodeSignature> nodes {
        makeNode("event.midi_input",
                 ir::Domain::event,
                 {},
                 {},
                 { { "note", "event.note" }, { "gate", "event.trigger" }, { "velocity", "control<f32>" } }),
        makeNode("event.note_to_frequency",
                 ir::Domain::event,
                 {},
                 { { "note", "event.note" } },
                 { { "frequency", "control<f32>" } }),
        makeNode("audio.oscillator",
                 ir::Domain::audio,
                 { { "waveform", "string", true }, { "frequency", "control<f32>", false } },
                 { { "frequency", "control<f32>" } },
                 { { "out", "audio<mono>" } }),
        makeNode("control.envelope.adsr",
                 ir::Domain::control,
                 { { "attack", "f32", true }, { "decay", "f32", true }, { "sustain", "f32", true }, { "release", "f32", true } },
                 { { "gate", "event.trigger" } },
                 { { "out", "control<f32>" } }),
        makeNode("audio.gain",
                 ir::Domain::audio,
                 {},
                 { { "in", "audio<mono>" }, { "gain", "control<f32>" } },
                 { { "out", "audio<mono>" } }),
        makeNode("audio.output",
                 ir::Domain::audio,
                 {},
                 { { "in", "audio<mono>" } },
                 {}),
        makeNode("audio.input",
                 ir::Domain::audio,
                 {},
                 {},
                 { { "out", "audio<mono>" } }),
        makeNode("control.constant",
                 ir::Domain::control,
                 { { "value", "f32", true } },
                 {},
                 { { "out", "control<f32>" } }),
        makeNode("control.lfo",
                 ir::Domain::control,
                 { { "rate_hz", "f32", true }, { "depth", "f32", true }, { "waveform", "string", false } },
                 {},
                 { { "out", "control<f32>" } }),
        makeNode("audio.filter.lowpass",
                 ir::Domain::audio,
                 { { "cutoff", "control<f32>", false }, { "resonance", "f32", false } },
                 { { "in", "audio<mono>" }, { "cutoff", "control<f32>" } },
                 { { "out", "audio<mono>" } })
    };

    return nodes;
}
} // namespace cw::patina
