#pragma once

#include <JuceHeader.h>

namespace cw::patina::ir
{
enum class Domain
{
    audio,
    control,
    event,
    worker
};

struct ValueRef
{
    enum class Kind
    {
        literal,
        graphParameter,
        localConstant,
        symbolic
    };

    Kind kind = Kind::literal;
    juce::var literalValue;
    juce::String referenceName;
};

struct Parameter
{
    juce::String name;
    juce::String typeText;
    bool hasDefaultValue = false;
    ValueRef defaultValue;
};

struct NodeArgument
{
    juce::String name;
    ValueRef value;
};

struct Node
{
    juce::String id;
    juce::String kind;
    Domain domain = Domain::audio;
    juce::Array<NodeArgument> arguments;
    int line = 0;
};

struct Edge
{
    juce::String sourceNode;
    juce::String sourcePort;
    juce::String destinationNode;
    juce::String destinationPort;
    int line = 0;
};

struct Graph
{
    juce::String name;
    juce::Array<Parameter> parameters;
    juce::Array<Node> nodes;
    juce::Array<Edge> edges;
};

struct Export
{
    juce::String kind;
    juce::String graphName;
};

struct Document
{
    juce::String schemaVersion;
    juce::String runtimeVersion;
    juce::String abiVersion;
    juce::String packageName;
    juce::String packageVersion;
    juce::Array<Graph> graphs;
    juce::Array<Export> exports;
};

juce::String toString(Domain domain);
juce::String describeValueRef(const ValueRef& valueRef);
} // namespace cw::patina::ir
