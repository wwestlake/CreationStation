#include "PatinaArtifactWriter.h"

namespace cw::patina
{
namespace
{
juce::var typeToVar(const TypeSpec& type)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("family", type.family);
    object->setProperty("raw", type.toString());

    juce::Array<juce::var> generics;
    for (const auto& generic : type.genericArguments)
        generics.add(generic);
    object->setProperty("genericArguments", juce::var(generics));

    return juce::var(object);
}

juce::var valueRefToVar(const ir::ValueRef& valueRef)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("kind", [&]() -> juce::String
    {
        switch (valueRef.kind)
        {
            case ir::ValueRef::Kind::literal: return "literal";
            case ir::ValueRef::Kind::graphParameter: return "graphParameter";
            case ir::ValueRef::Kind::localConstant: return "localConstant";
            case ir::ValueRef::Kind::symbolic: return "symbolic";
        }

        return "literal";
    }());

    if (valueRef.kind == ir::ValueRef::Kind::literal)
        object->setProperty("value", valueRef.literalValue);
    else
        object->setProperty("reference", valueRef.referenceName);

    return juce::var(object);
}

juce::var parameterToVar(const ir::Parameter& parameter)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", parameter.name);
    object->setProperty("type", parameter.typeText);
    object->setProperty("typeSpec", typeToVar(parameter.type));
    object->setProperty("hasDefaultValue", parameter.hasDefaultValue);

    if (parameter.hasDefaultValue)
        object->setProperty("defaultValue", valueRefToVar(parameter.defaultValue));

    return juce::var(object);
}

juce::var nodeArgumentToVar(const ir::NodeArgument& argument)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", argument.name);
    object->setProperty("value", valueRefToVar(argument.value));
    object->setProperty("valueType", typeToVar(argument.valueType));
    return juce::var(object);
}

juce::var portToVar(const ir::Port& port)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", port.name);
    object->setProperty("type", typeToVar(port.type));
    return juce::var(object);
}

juce::var nodeToVar(const ir::Node& node)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", node.id);
    object->setProperty("kind", node.kind);
    object->setProperty("domain", ir::toString(node.domain));
    object->setProperty("line", node.line);

    juce::Array<juce::var> arguments;
    for (const auto& argument : node.arguments)
        arguments.add(nodeArgumentToVar(argument));
    object->setProperty("arguments", juce::var(arguments));

    juce::Array<juce::var> inputs;
    for (const auto& input : node.inputs)
        inputs.add(portToVar(input));
    object->setProperty("inputs", juce::var(inputs));

    juce::Array<juce::var> outputs;
    for (const auto& output : node.outputs)
        outputs.add(portToVar(output));
    object->setProperty("outputs", juce::var(outputs));

    return juce::var(object);
}

juce::var edgeToVar(const ir::Edge& edge)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("sourceNode", edge.sourceNode);
    object->setProperty("sourcePort", edge.sourcePort);
    object->setProperty("destinationNode", edge.destinationNode);
    object->setProperty("destinationPort", edge.destinationPort);
    object->setProperty("signalType", typeToVar(edge.signalType));
    object->setProperty("line", edge.line);
    return juce::var(object);
}

juce::var graphToVar(const ir::Graph& graph)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", graph.name);

    juce::Array<juce::var> parameters;
    for (const auto& parameter : graph.parameters)
        parameters.add(parameterToVar(parameter));
    object->setProperty("parameters", juce::var(parameters));

    juce::Array<juce::var> nodes;
    for (const auto& node : graph.nodes)
        nodes.add(nodeToVar(node));
    object->setProperty("nodes", juce::var(nodes));

    juce::Array<juce::var> edges;
    for (const auto& edge : graph.edges)
        edges.add(edgeToVar(edge));
    object->setProperty("edges", juce::var(edges));

    return juce::var(object);
}

juce::var exportToVar(const ir::Export& exportDeclaration)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("kind", exportDeclaration.kind);
    object->setProperty("graph", exportDeclaration.graphName);
    return juce::var(object);
}
} // namespace

juce::String ArtifactWriter::writeJson(const ir::Document& document) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", document.schemaVersion);
    root->setProperty("runtimeVersion", document.runtimeVersion);
    root->setProperty("abiVersion", document.abiVersion);
    root->setProperty("package", document.packageName);
    root->setProperty("version", document.packageVersion);

    juce::Array<juce::var> graphs;
    for (const auto& graph : document.graphs)
        graphs.add(graphToVar(graph));
    root->setProperty("graphs", juce::var(graphs));

    juce::Array<juce::var> exports;
    for (const auto& exportDeclaration : document.exports)
        exports.add(exportToVar(exportDeclaration));
    root->setProperty("exports", juce::var(exports));

    return juce::JSON::toString(juce::var(root), true);
}
} // namespace cw::patina
