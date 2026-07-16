#include "DslCompiler.h"
#include "PatinaArtifactWriter.h"
#include "PatinaBinder.h"
#include "PatinaIr.h"
#include "PatinaLexer.h"
#include "PatinaLowering.h"
#include "PatinaParser.h"

namespace cw
{
DslModule DslCompiler::compile(const juce::String& source) const
{
    DslModule module;

    if (source.trim().isEmpty())
    {
        module.diagnostics.add({ 1, "The DSL buffer is empty." });
        return module;
    }

    patina::Lexer lexer;
    auto lexResult = lexer.tokenize(source);
    if (! lexResult.diagnostics.isEmpty())
    {
        for (const auto& diagnostic : lexResult.diagnostics)
            module.diagnostics.add({ diagnostic.line, "Column " + juce::String(diagnostic.column) + ": " + diagnostic.message });
        return module;
    }

    patina::Parser parser;
    auto parseResult = parser.parse(lexResult.tokens);
    if (! parseResult.diagnostics.isEmpty())
    {
        for (const auto& diagnostic : parseResult.diagnostics)
            module.diagnostics.add({ diagnostic.line, "Column " + juce::String(diagnostic.column) + ": " + diagnostic.message });
        return module;
    }

    patina::Binder binder;
    auto bindResult = binder.bind(parseResult.sourceFile);
    if (! bindResult.diagnostics.isEmpty())
    {
        for (const auto& diagnostic : bindResult.diagnostics)
            module.diagnostics.add({ diagnostic.line, diagnostic.message });
        return module;
    }

    patina::Lowerer lowerer;
    auto loweringResult = lowerer.lower(bindResult.sourceFile);
    if (! loweringResult.diagnostics.isEmpty())
    {
        for (const auto& diagnostic : loweringResult.diagnostics)
            module.diagnostics.add({ diagnostic.line, diagnostic.message });
        return module;
    }

    module.surfaceAst = parseResult.sourceFile;
    module.boundAst = bindResult.sourceFile;
    module.semanticIr = loweringResult.document;
    patina::ArtifactWriter artifactWriter;
    module.artifactJson = artifactWriter.writeJson(module.semanticIr);
    module.packageName = parseResult.sourceFile.packageName;
    module.version = parseResult.sourceFile.version;
    module.graphCount = parseResult.sourceFile.graphs.size();
    module.importCount = parseResult.sourceFile.imports.size();
    module.exportCount = parseResult.sourceFile.exports.size();

    for (const auto& graph : parseResult.sourceFile.graphs)
    {
        module.sourceCount += graph.nodes.size();
        module.connectionCount += graph.connections.size();
        module.parameterCount += graph.parameters.size();
        module.letCount += graph.lets.size();
    }

    juce::StringArray debugLines;
    if (module.packageName.isNotEmpty())
        debugLines.add("Package: " + module.packageName);
    if (module.version.isNotEmpty())
        debugLines.add("Version: " + module.version);

    if (! parseResult.sourceFile.imports.isEmpty())
    {
        debugLines.add("Imports:");
        for (const auto& importDeclaration : parseResult.sourceFile.imports)
            debugLines.add("  - " + importDeclaration.path
                           + (importDeclaration.alias.isNotEmpty() ? " as " + importDeclaration.alias : ""));
    }

    for (const auto& graph : parseResult.sourceFile.graphs)
    {
        debugLines.add("Graph " + graph.name + ":");
        for (const auto& parameter : graph.parameters)
            debugLines.add("  param " + parameter.name + ": " + parameter.type.toString());
        for (const auto& letDeclaration : graph.lets)
            debugLines.add("  let " + letDeclaration.name + " = " + letDeclaration.value.text);
        for (const auto& node : graph.nodes)
            debugLines.add("  node " + node.name + " = " + node.callee.toString() + "(" + juce::String(node.arguments.size()) + " args)");
        for (const auto& connection : graph.connections)
            debugLines.add("  connect " + connection.source.toString() + " -> " + connection.destination.toString());
    }

    for (const auto& exportDeclaration : parseResult.sourceFile.exports)
        debugLines.add("Export " + exportDeclaration.kind + " " + exportDeclaration.graphName);

    debugLines.add("");
    debugLines.add("Semantic IR:");
    for (const auto& graph : loweringResult.document.graphs)
    {
        debugLines.add("  graph " + graph.name + " {");
        for (const auto& parameter : graph.parameters)
        {
            auto parameterLine = "    param " + parameter.name + ": " + parameter.typeText;
            if (parameter.hasDefaultValue)
                parameterLine += " = " + patina::ir::describeValueRef(parameter.defaultValue);
            debugLines.add(parameterLine);
        }
        for (const auto& node : graph.nodes)
        {
            debugLines.add("    node " + node.id + " :: " + node.kind + " [" + patina::ir::toString(node.domain) + "]");
            for (const auto& argument : node.arguments)
                debugLines.add("      arg " + argument.name + " = " + patina::ir::describeValueRef(argument.value));
        }
        for (const auto& edge : graph.edges)
            debugLines.add("    edge " + edge.sourceNode + "." + edge.sourcePort + " -> " + edge.destinationNode + "." + edge.destinationPort);
        debugLines.add("  }");
    }

    module.debugTree = debugLines.joinIntoString("\n");
    module.success = true;
    module.summary = juce::String("Patina parse OK\n")
                   + "Graphs: " + juce::String(module.graphCount)
                   + "  |  Nodes: " + juce::String(module.sourceCount)
                   + "  |  Connections: " + juce::String(module.connectionCount)
                   + "  |  Params: " + juce::String(module.parameterCount)
                   + "  |  Lets: " + juce::String(module.letCount)
                   + "  |  Exports: " + juce::String(module.exportCount)
                   + "\nStages: lex -> parse -> bind -> lower"
                   + "\n\n"
                   + module.debugTree
                   + "\n\nArtifact JSON:\n"
                   + module.artifactJson;
    return module;
}
} // namespace cw
