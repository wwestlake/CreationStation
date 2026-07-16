#include "PatinaLowering.h"

namespace cw::patina
{
namespace
{
ir::Domain domainForCallee(const QualifiedName& callee)
{
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
} // namespace

LoweringResult Lowerer::lower(const BoundSourceFile& sourceFile) const
{
    LoweringResult result;
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

            for (const auto& argument : node.arguments)
                loweredNode.arguments.add({ argument.name, lowerValue(argument.value) });

            loweredGraph.nodes.add(loweredNode);
        }

        for (const auto& connection : graph.connections)
            loweredGraph.edges.add({ connection.source.nodeName, connection.source.portName,
                                     connection.destination.nodeName, connection.destination.portName,
                                     connection.line });

        result.document.graphs.add(loweredGraph);
    }

    for (const auto& exportDeclaration : sourceFile.exports)
        result.document.exports.add({ exportDeclaration.kind, exportDeclaration.graphName });

    return result;
}
} // namespace cw::patina
