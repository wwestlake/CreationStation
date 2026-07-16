#include "DslCompiler.h"
#include "PatinaLexer.h"
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

    module.surfaceAst = parseResult.sourceFile;
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

    module.debugTree = debugLines.joinIntoString("\n");
    module.success = true;
    module.summary = juce::String("Patina parse OK\n")
                   + "Graphs: " + juce::String(module.graphCount)
                   + "  |  Nodes: " + juce::String(module.sourceCount)
                   + "  |  Connections: " + juce::String(module.connectionCount)
                   + "  |  Params: " + juce::String(module.parameterCount)
                   + "  |  Lets: " + juce::String(module.letCount)
                   + "  |  Exports: " + juce::String(module.exportCount)
                   + "\n\n"
                   + module.debugTree;
    return module;
}
} // namespace cw
