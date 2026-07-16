#include "PatinaLowering.h"
#include "PatinaBuiltins.h"

namespace cw::patina
{
namespace
{
ir::Domain domainForCallee(const QualifiedName& callee)
{
    BuiltinsRegistry builtins;
    if (const auto* signature = builtins.findNode(callee))
        return signature->domain;

    if (callee.segments.isEmpty())
        return ir::Domain::worker;

    auto root = callee.segments[0];
    if (root == "audio")
        return ir::Domain::audio;
    if (root == "control")
        return ir::Domain::control;
    if (root == "event")
        return ir::Domain::event;
    return ir::Domain::worker;
}

ir::ValueRef lowerValue(const BoundValue& value)
{
    ir::ValueRef lowered;
    switch (value.kind)
    {
        case BoundValue::Kind::literal:
            lowered.kind = ir::ValueRef::Kind::literal;
            lowered.literalValue = value.literalValue;
            break;
        case BoundValue::Kind::letReference:
            lowered.kind = ir::ValueRef::Kind::localConstant;
            lowered.referenceName = value.referenceName;
            break;
        case BoundValue::Kind::parameterReference:
            lowered.kind = ir::ValueRef::Kind::graphParameter;
            lowered.referenceName = value.referenceName;
            break;
        case BoundValue::Kind::symbolicReference:
            lowered.kind = ir::ValueRef::Kind::symbolic;
            lowered.referenceName = value.referenceName;
            break;
    }

    return lowered;
}

TypeSpec inferBoundValueType(const BoundValue& value)
{
    TypeSystem typeSystem;

    switch (value.kind)
    {
        case BoundValue::Kind::literal:
            if (value.literalValue.isString())
                return typeSystem.parseType("string");
            if (value.literalValue.isBool())
                return typeSystem.parseType("bool");
            if (value.literalValue.isInt() || value.literalValue.isInt64())
                return typeSystem.parseType("i64");
            if (value.literalValue.isDouble())
                return typeSystem.parseType("f32");
            return {};
        case BoundValue::Kind::letReference:
        case BoundValue::Kind::parameterReference:
        case BoundValue::Kind::symbolicReference:
            return {};
    }

    return {};
}
} // namespace

LoweringResult Lowerer::lower(const BoundSourceFile& sourceFile) const
{
    LoweringResult result;
    BuiltinsRegistry builtins;
    TypeSystem typeSystem;
    result.document.packageName = sourceFile.packageName;
    result.document.packageVersion = sourceFile.version;
    result.document.schemaVersion = "0.1";
    result.document.runtimeVersion = "0.1";
    result.document.abiVersion = "0.1";

    for (const auto& graph : sourceFile.graphs)
    {
        ir::Graph loweredGraph;
        loweredGraph.name = graph.name;

        for (const auto& parameter : graph.parameters)
        {
            ir::Parameter loweredParameter;
            loweredParameter.name = parameter.name;
            loweredParameter.typeText = parameter.typeText;
            loweredParameter.type = typeSystem.parseType(parameter.typeText);
            loweredParameter.hasDefaultValue = parameter.hasDefaultValue;
            if (parameter.hasDefaultValue)
                loweredParameter.defaultValue = lowerValue(parameter.defaultValue);
            loweredGraph.parameters.add(loweredParameter);
        }

        for (const auto& node : graph.nodes)
        {
            ir::Node loweredNode;
            loweredNode.id = node.name;
            loweredNode.kind = node.callee.toString();
            loweredNode.domain = domainForCallee(node.callee);
            loweredNode.line = node.line;

            if (const auto* signature = builtins.findNode(node.callee))
            {
                for (const auto& input : signature->inputs)
                    loweredNode.inputs.add({ input.name, input.parsedValueType });

                for (const auto& output : signature->outputs)
                    loweredNode.outputs.add({ output.name, output.parsedValueType });
            }

            for (const auto& argument : node.arguments)
            {
                TypeSpec argumentType = inferBoundValueType(argument.value);
                if (const auto* signature = builtins.findNode(node.callee))
                    if (const auto* argumentSignature = builtins.findArgument(*signature, argument.name))
                        argumentType = argumentSignature->parsedValueType;

                loweredNode.arguments.add({ argument.name, lowerValue(argument.value), argumentType });
            }

            loweredGraph.nodes.add(loweredNode);
        }

        for (const auto& connection : graph.connections)
        {
            ir::Edge loweredEdge;
            loweredEdge.sourceNode = connection.source.nodeName;
            loweredEdge.sourcePort = connection.source.portName;
            loweredEdge.destinationNode = connection.destination.nodeName;
            loweredEdge.destinationPort = connection.destination.portName;
            loweredEdge.line = connection.line;

            for (const auto& node : loweredGraph.nodes)
            {
                if (node.id == loweredEdge.sourceNode)
                {
                    for (const auto& output : node.outputs)
                        if (output.name == loweredEdge.sourcePort)
                            loweredEdge.signalType = output.type;
                }
            }

            loweredGraph.edges.add(loweredEdge);
        }

        result.document.graphs.add(loweredGraph);
    }

    for (const auto& exportDeclaration : sourceFile.exports)
        result.document.exports.add({ exportDeclaration.kind, exportDeclaration.graphName });

    return result;
}
} // namespace cw::patina
