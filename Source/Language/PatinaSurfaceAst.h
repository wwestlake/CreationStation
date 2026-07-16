#pragma once

#include <JuceHeader.h>

namespace cw::patina
{
struct QualifiedName
{
    juce::StringArray segments;

    bool isEmpty() const noexcept { return segments.isEmpty(); }
    juce::String toString() const { return segments.joinIntoString("."); }
};

struct Expression
{
    enum class Kind
    {
        identifier,
        qualifiedName,
        stringLiteral,
        integerLiteral,
        floatLiteral,
        booleanLiteral
    };

    Kind kind = Kind::identifier;
    juce::String text;
    QualifiedName qualifiedName;
};

struct NamedArgument
{
    juce::String name;
    Expression value;
};

struct TypeExpression
{
    QualifiedName baseName;
    juce::StringArray genericArguments;

    juce::String toString() const
    {
        if (genericArguments.isEmpty())
            return baseName.toString();

        return baseName.toString() + "<" + genericArguments.joinIntoString(", ") + ">";
    }
};

struct ImportDeclaration
{
    juce::String path;
    juce::String alias;
    int line = 0;
};

struct LetDeclaration
{
    juce::String name;
    Expression value;
    int line = 0;
};

struct ParameterDeclaration
{
    juce::String name;
    TypeExpression type;
    bool hasDefaultValue = false;
    Expression defaultValue;
    int line = 0;
};

struct NodeDeclaration
{
    juce::String name;
    QualifiedName callee;
    juce::Array<NamedArgument> arguments;
    int line = 0;
};

struct PortReference
{
    juce::String nodeName;
    juce::String portName;
    int line = 0;

    juce::String toString() const { return nodeName + "." + portName; }
};

struct ConnectionDeclaration
{
    PortReference source;
    PortReference destination;
    int line = 0;
};

struct GraphDeclaration
{
    juce::String name;
    juce::Array<LetDeclaration> lets;
    juce::Array<ParameterDeclaration> parameters;
    juce::Array<NodeDeclaration> nodes;
    juce::Array<ConnectionDeclaration> connections;
    int line = 0;
};

struct ExportDeclaration
{
    juce::String kind;
    juce::String graphName;
    int line = 0;
};

struct SourceFile
{
    juce::String packageName;
    juce::String version;
    juce::Array<ImportDeclaration> imports;
    juce::Array<GraphDeclaration> graphs;
    juce::Array<ExportDeclaration> exports;
};
} // namespace cw::patina
