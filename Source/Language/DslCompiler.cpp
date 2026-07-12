#include "DslCompiler.h"

namespace cw
{
namespace
{
bool hasBalancedDelimiters(const juce::String& text)
{
    int roundCount = 0;
    int squareCount = 0;
    int braceCount = 0;

    for (auto character : text)
    {
        switch (character)
        {
            case '(': ++roundCount; break;
            case ')': --roundCount; break;
            case '[': ++squareCount; break;
            case ']': --squareCount; break;
            case '{': ++braceCount; break;
            case '}': --braceCount; break;
            default: break;
        }

        if (roundCount < 0 || squareCount < 0 || braceCount < 0)
            return false;
    }

    return roundCount == 0 && squareCount == 0 && braceCount == 0;
}
} // namespace

bool DslCompiler::isRecognisedStatement(const juce::String& line)
{
    auto trimmed = line.trimStart();

    if (trimmed.isEmpty() || trimmed.startsWithChar('#'))
        return true;

    static const juce::StringArray prefixes
    {
        "let ", "fn ", "graph ", "node ", "route ", "emit ",
        "source ", "effect ", "sink ", "connect ", "modulate ", "automation "
    };
    for (const auto& prefix : prefixes)
        if (trimmed.startsWithIgnoreCase(prefix))
            return true;

    return false;
}

DslModule DslCompiler::compile(const juce::String& source) const
{
    DslModule module;

    if (source.trim().isEmpty())
    {
        module.diagnostics.add({ 1, "The DSL buffer is empty." });
        return module;
    }

    if (! hasBalancedDelimiters(source))
    {
        module.diagnostics.add({ 1, "Unbalanced parentheses, brackets, or braces." });
        return module;
    }

    auto lines = juce::StringArray::fromLines(source);
    int lineNumber = 1;
    for (const auto& line : lines)
    {
        if (! isRecognisedStatement(line))
        {
            module.diagnostics.add({ lineNumber, "Unknown statement. Start lines with source, effect, sink, connect, modulate, route, let, fn, graph, node, or emit." });
            return module;
        }

        auto trimmed = line.trimStart();
        if (trimmed.startsWithIgnoreCase("source "))
            ++module.sourceCount;
        else if (trimmed.startsWithIgnoreCase("effect "))
            ++module.effectCount;
        else if (trimmed.startsWithIgnoreCase("sink "))
            ++module.sinkCount;
        else if (trimmed.startsWithIgnoreCase("connect ") || trimmed.startsWithIgnoreCase("route "))
            ++module.connectionCount;
        else if (trimmed.startsWithIgnoreCase("modulate ") || trimmed.startsWithIgnoreCase("automation "))
            ++module.modulationCount;

        ++lineNumber;
    }

    module.success = true;
    module.summary = "Parsed " + juce::String(lines.size()) + " lines: "
                   + juce::String(module.sourceCount) + " sources, "
                   + juce::String(module.effectCount) + " effects, "
                   + juce::String(module.sinkCount) + " sinks, "
                   + juce::String(module.connectionCount) + " routes, "
                   + juce::String(module.modulationCount) + " modulation lanes.";
    return module;
}
} // namespace cw
