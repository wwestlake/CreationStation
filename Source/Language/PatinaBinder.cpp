#include "PatinaBinder.h"

namespace cw::patina
{
namespace
{
juce::var literalValueFor(const Expression& expression)
{
    switch (expression.kind)
    {
        case Expression::Kind::stringLiteral: return expression.text;
        case Expression::Kind::integerLiteral: return expression.text.getLargeIntValue();
        case Expression::Kind::floatLiteral: return expression.text.getDoubleValue();
        case Expression::Kind::booleanLiteral: return expression.text.equalsIgnoreCase("true");
        case Expression::Kind::identifier:
        case Expression::Kind::qualifiedName:
            break;
    }

    return {};
}

bool isBuiltinRoot(const juce::String& text)
{
    return text == "audio" || text == "control" || text == "event";
}

class BinderState
{
public:
    explicit BinderState(const SourceFile& inputSourceFile) : sourceFile(inputSourceFile) {}

    BindResult run()
    {
        result.sourceFile.packageName = sourceFile.packageName;
        result.sourceFile.version = sourceFile.version;
        result.sourceFile.imports = sourceFile.imports;

        juce::StringArray importAliases;
        for (const auto& importDeclaration : sourceFile.imports)
        {
            if (importDeclaration.alias.isNotEmpty())
            {
                if (importAliases.contains(importDeclaration.alias))
                    result.diagnostics.add({ importDeclaration.line, 1, "Duplicate import alias '" + importDeclaration.alias + "'." });
                else
                    importAliases.add(importDeclaration.alias);
            }
        }

        for (const auto& graph : sourceFile.graphs)
            bindGraph(graph, importAliases);

        for (const auto& exportDeclaration : sourceFile.exports)
            result.sourceFile.exports.add({ exportDeclaration.kind, exportDeclaration.graphName, exportDeclaration.line });

        validateExports();
        return result;
    }

private:
    const SourceFile& sourceFile;
    BuiltinsRegistry builtins;
    BindResult result;

    void bindGraph(const GraphDeclaration& graph, const juce::StringArray& importAliases)
    {
        BoundGraphDeclaration boundGraph;
        boundGraph.name = graph.name;
        boundGraph.line = graph.line;

        if (seenGraphNames.contains(graph.name))
            result.diagnostics.add({ graph.line, 1, "Duplicate graph '" + graph.name + "'." });
        else
            seenGraphNames.add(graph.name);

        juce::StringArray parameterNames;
        juce::HashMap<juce::String, juce::String> parameterTypes;
        for (const auto& parameter : graph.parameters)
        {
            if (parameterNames.contains(parameter.name))
            {
                result.diagnostics.add({ parameter.line, 1, "Duplicate parameter '" + parameter.name + "' in graph '" + graph.name + "'." });
                continue;
            }

            parameterNames.add(parameter.name);
            parameterTypes.set(parameter.name, parameter.type.toString());

            BoundParameterDeclaration boundParameter;
            boundParameter.name = parameter.name;
            boundParameter.typeText = parameter.type.toString();
            boundParameter.line = parameter.line;
            boundParameter.hasDefaultValue = parameter.hasDefaultValue;

            if (parameter.hasDefaultValue)
                boundParameter.defaultValue = bindExpression(parameter.defaultValue, graph, parameterNames, {}, parameter.line);

            boundGraph.parameters.add(boundParameter);
        }

        juce::StringArray letNames;
        juce::HashMap<juce::String, juce::String> letTypes;
        for (const auto& letDeclaration : graph.lets)
        {
            if (parameterNames.contains(letDeclaration.name) || letNames.contains(letDeclaration.name))
            {
                result.diagnostics.add({ letDeclaration.line, 1, "Duplicate local name '" + letDeclaration.name + "' in graph '" + graph.name + "'." });
                continue;
            }

            BoundLetDeclaration boundLet;
            boundLet.name = letDeclaration.name;
            boundLet.line = letDeclaration.line;
            boundLet.value = bindExpression(letDeclaration.value, graph, parameterNames, letNames, letDeclaration.line);
            boundGraph.lets.add(boundLet);
            letNames.add(letDeclaration.name);
            letTypes.set(letDeclaration.name, inferValueType(boundLet.value, parameterTypes, letTypes));
        }

        juce::StringArray nodeNames;
        juce::HashMap<juce::String, const BuiltinNodeSignature*> nodeSignatures;
        for (const auto& node : graph.nodes)
        {
            if (parameterNames.contains(node.name) || letNames.contains(node.name) || nodeNames.contains(node.name))
            {
                result.diagnostics.add({ node.line, 1, "Duplicate node name '" + node.name + "' in graph '" + graph.name + "'." });
                continue;
            }

            BoundNodeDeclaration boundNode;
            boundNode.name = node.name;
            boundNode.line = node.line;
            boundNode.callee = node.callee;

            if (! node.callee.segments.isEmpty())
            {
                auto firstSegment = node.callee.segments[0];
                if (! isBuiltinRoot(firstSegment))
                {
                    if (firstSegment.containsChar('@'))
                        boundNode.resolvedImportPath = firstSegment;
                    else if (importAliases.contains(firstSegment))
                        boundNode.resolvedImportPath = firstSegment;
                }
            }

            if (const auto* builtinSignature = builtins.findNode(node.callee))
            {
                boundNode.builtinQualifiedName = builtinSignature->qualifiedName;
                validateBuiltinNode(node, *builtinSignature, parameterTypes, letTypes);
                nodeSignatures.set(node.name, builtinSignature);
            }
            else if (boundNode.resolvedImportPath.isEmpty())
            {
                result.diagnostics.add({ node.line, 1, "Unknown builtin or imported node '" + node.callee.toString() + "'." });
            }

            for (const auto& argument : node.arguments)
            {
                BoundNamedArgument boundArgument;
                boundArgument.name = argument.name;
                boundArgument.value = bindExpression(argument.value, graph, parameterNames, letNames, node.line);
                boundNode.arguments.add(boundArgument);
            }

            boundGraph.nodes.add(boundNode);
            nodeNames.add(node.name);
        }

        for (const auto& connection : graph.connections)
        {
            boundGraph.connections.add({ connection.source, connection.destination, connection.line });
            validateConnection(graph, connection, nodeSignatures);
        }

        result.sourceFile.graphs.add(boundGraph);
    }

    BoundValue bindExpression(const Expression& expression,
                              const GraphDeclaration&,
                              const juce::StringArray& parameterNames,
                              const juce::StringArray& letNames,
                              int line)
    {
        BoundValue value;
        switch (expression.kind)
        {
            case Expression::Kind::stringLiteral:
            case Expression::Kind::integerLiteral:
            case Expression::Kind::floatLiteral:
            case Expression::Kind::booleanLiteral:
                value.kind = BoundValue::Kind::literal;
                value.literalValue = literalValueFor(expression);
                return value;

            case Expression::Kind::identifier:
                if (letNames.contains(expression.text))
                {
                    value.kind = BoundValue::Kind::letReference;
                    value.referenceName = expression.text;
                    return value;
                }

                if (parameterNames.contains(expression.text))
                {
                    value.kind = BoundValue::Kind::parameterReference;
                    value.referenceName = expression.text;
                    return value;
                }

                result.diagnostics.add({ line, 1, "Unknown identifier '" + expression.text + "' in expression." });
                value.kind = BoundValue::Kind::symbolicReference;
                value.referenceName = expression.text;
                return value;

            case Expression::Kind::qualifiedName:
                value.kind = BoundValue::Kind::symbolicReference;
                value.referenceName = expression.qualifiedName.toString();
                return value;
        }

        return value;
    }

    void validateExports()
    {
        juce::StringArray graphNames;
        for (const auto& graph : result.sourceFile.graphs)
            graphNames.add(graph.name);

        for (const auto& exportDeclaration : result.sourceFile.exports)
            if (! graphNames.contains(exportDeclaration.graphName))
                result.diagnostics.add({ exportDeclaration.line, 1, "Export references unknown graph '" + exportDeclaration.graphName + "'." });
    }

    void validateBuiltinNode(const NodeDeclaration& node,
                             const BuiltinNodeSignature& signature,
                             const juce::HashMap<juce::String, juce::String>& parameterTypes,
                             const juce::HashMap<juce::String, juce::String>& letTypes)
    {
        juce::StringArray seenArguments;
        for (const auto& argument : node.arguments)
        {
            if (seenArguments.contains(argument.name))
            {
                result.diagnostics.add({ node.line, 1, "Duplicate argument '" + argument.name + "' for node '" + node.name + "'." });
                continue;
            }

            seenArguments.add(argument.name);

            const auto* argumentSignature = builtins.findArgument(signature, argument.name);
            if (argumentSignature == nullptr)
            {
                result.diagnostics.add({ node.line, 1, "Node '" + node.name + "' does not accept argument '" + argument.name + "'." });
                continue;
            }

            auto actualType = inferExpressionType(argument.value, parameterTypes, letTypes);
            if (actualType.isNotEmpty() && ! builtins.isValueAssignableTo(actualType, argumentSignature->valueType))
            {
                result.diagnostics.add({ node.line, 1, "Argument '" + argument.name + "' for node '" + node.name
                                                + "' expects " + argumentSignature->valueType
                                                + " but received " + actualType + "." });
            }
        }

        for (const auto& argumentSignature : signature.arguments)
            if (argumentSignature.required && ! seenArguments.contains(argumentSignature.name))
                result.diagnostics.add({ node.line, 1, "Node '" + node.name + "' is missing required argument '" + argumentSignature.name + "'." });
    }

    void validateConnection(const GraphDeclaration& graph,
                            const ConnectionDeclaration& connection,
                            const juce::HashMap<juce::String, const BuiltinNodeSignature*>& nodeSignatures)
    {
        const auto* sourceSignature = nodeSignatures.contains(connection.source.nodeName) ? nodeSignatures[connection.source.nodeName] : nullptr;
        const auto* destinationSignature = nodeSignatures.contains(connection.destination.nodeName) ? nodeSignatures[connection.destination.nodeName] : nullptr;

        if (sourceSignature == nullptr)
        {
            if (! hasNodeNamed(graph, connection.source.nodeName))
                result.diagnostics.add({ connection.line, 1, "Connection source node '" + connection.source.nodeName + "' does not exist." });
            return;
        }

        if (destinationSignature == nullptr)
        {
            if (! hasNodeNamed(graph, connection.destination.nodeName))
                result.diagnostics.add({ connection.line, 1, "Connection destination node '" + connection.destination.nodeName + "' does not exist." });
            return;
        }

        const auto* sourcePort = builtins.findOutput(*sourceSignature, connection.source.portName);
        if (sourcePort == nullptr)
        {
            result.diagnostics.add({ connection.line, 1, "Node '" + connection.source.nodeName + "' has no output port '" + connection.source.portName + "'." });
            return;
        }

        const auto* destinationPort = builtins.findInput(*destinationSignature, connection.destination.portName);
        if (destinationPort == nullptr)
        {
            result.diagnostics.add({ connection.line, 1, "Node '" + connection.destination.nodeName + "' has no input port '" + connection.destination.portName + "'." });
            return;
        }

        if (! builtins.isValueAssignableTo(sourcePort->valueType, destinationPort->valueType))
        {
            result.diagnostics.add({ connection.line, 1, "Cannot connect " + connection.source.toString()
                                            + " (" + sourcePort->valueType + ") to "
                                            + connection.destination.toString() + " (" + destinationPort->valueType + ")." });
        }
    }

    juce::String inferExpressionType(const Expression& expression,
                                     const juce::HashMap<juce::String, juce::String>& parameterTypes,
                                     const juce::HashMap<juce::String, juce::String>& letTypes) const
    {
        switch (expression.kind)
        {
            case Expression::Kind::stringLiteral: return "string";
            case Expression::Kind::integerLiteral: return "i64";
            case Expression::Kind::floatLiteral: return "f32";
            case Expression::Kind::booleanLiteral: return "bool";
            case Expression::Kind::identifier:
                if (letTypes.contains(expression.text))
                    return letTypes[expression.text];
                if (parameterTypes.contains(expression.text))
                    return parameterTypes[expression.text];
                return {};
            case Expression::Kind::qualifiedName:
                return expression.qualifiedName.toString();
        }

        return {};
    }

    juce::String inferValueType(const BoundValue& value,
                                const juce::HashMap<juce::String, juce::String>& parameterTypes,
                                const juce::HashMap<juce::String, juce::String>& letTypes) const
    {
        switch (value.kind)
        {
            case BoundValue::Kind::literal:
                if (value.literalValue.isString())
                    return "string";
                if (value.literalValue.isBool())
                    return "bool";
                if (value.literalValue.isInt() || value.literalValue.isInt64())
                    return "i64";
                if (value.literalValue.isDouble())
                    return "f32";
                return {};
            case BoundValue::Kind::letReference:
                return letTypes.contains(value.referenceName) ? letTypes[value.referenceName] : juce::String();
            case BoundValue::Kind::parameterReference:
                return parameterTypes.contains(value.referenceName) ? parameterTypes[value.referenceName] : juce::String();
            case BoundValue::Kind::symbolicReference:
                return value.referenceName;
        }

        return {};
    }

    bool hasNodeNamed(const GraphDeclaration& graph, const juce::String& nodeName) const
    {
        for (const auto& node : graph.nodes)
            if (node.name == nodeName)
                return true;

        return false;
    }

    juce::StringArray seenGraphNames;
};
} // namespace

BindResult Binder::bind(const SourceFile& sourceFile) const
{
    BinderState binderState(sourceFile);
    return binderState.run();
}
} // namespace cw::patina
