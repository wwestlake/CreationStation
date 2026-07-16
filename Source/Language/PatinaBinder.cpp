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
    BindResult result;

    void bindGraph(const GraphDeclaration& graph, const juce::StringArray& importAliases)
    {
        BoundGraphDeclaration boundGraph;
        boundGraph.name = graph.name;
        boundGraph.line = graph.line;

        juce::StringArray parameterNames;
        for (const auto& parameter : graph.parameters)
        {
            parameterNames.add(parameter.name);

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
        for (const auto& letDeclaration : graph.lets)
        {
            BoundLetDeclaration boundLet;
            boundLet.name = letDeclaration.name;
            boundLet.line = letDeclaration.line;
            boundLet.value = bindExpression(letDeclaration.value, graph, parameterNames, letNames, letDeclaration.line);
            boundGraph.lets.add(boundLet);
            letNames.add(letDeclaration.name);
        }

        juce::StringArray nodeNames;
        for (const auto& node : graph.nodes)
        {
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
            boundGraph.connections.add({ connection.source, connection.destination, connection.line });

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
};
} // namespace

BindResult Binder::bind(const SourceFile& sourceFile) const
{
    BinderState binderState(sourceFile);
    return binderState.run();
}
} // namespace cw::patina
