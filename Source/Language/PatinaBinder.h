#pragma once

#include <JuceHeader.h>
#include "PatinaBuiltins.h"
#include "PatinaSurfaceAst.h"

namespace cw::patina
{
struct BindDiagnostic
{
    int line = 0;
    int column = 0;
    juce::String message;
};

struct BoundValue
{
    enum class Kind
    {
        literal,
        letReference,
        parameterReference,
        symbolicReference
    };

    Kind kind = Kind::literal;
    juce::var literalValue;
    juce::String referenceName;
};

struct BoundNamedArgument
{
    juce::String name;
    BoundValue value;
};

struct BoundLetDeclaration
{
    juce::String name;
    BoundValue value;
    int line = 0;
};

struct BoundParameterDeclaration
{
    juce::String name;
    juce::String typeText;
    bool hasDefaultValue = false;
    BoundValue defaultValue;
    int line = 0;
};

struct BoundNodeDeclaration
{
    juce::String name;
    QualifiedName callee;
    juce::String resolvedImportPath;
    juce::String builtinQualifiedName;
    juce::Array<BoundNamedArgument> arguments;
    int line = 0;
};

struct BoundConnectionDeclaration
{
    PortReference source;
    PortReference destination;
    int line = 0;
};

struct BoundGraphDeclaration
{
    juce::String name;
    juce::Array<BoundLetDeclaration> lets;
    juce::Array<BoundParameterDeclaration> parameters;
    juce::Array<BoundNodeDeclaration> nodes;
    juce::Array<BoundConnectionDeclaration> connections;
    int line = 0;
};

struct BoundExportDeclaration
{
    juce::String kind;
    juce::String graphName;
    int line = 0;
};

struct BoundSourceFile
{
    juce::String packageName;
    juce::String version;
    juce::Array<ImportDeclaration> imports;
    juce::Array<BoundGraphDeclaration> graphs;
    juce::Array<BoundExportDeclaration> exports;
};

struct BindResult
{
    BoundSourceFile sourceFile;
    juce::Array<BindDiagnostic> diagnostics;
};

class Binder final
{
public:
    BindResult bind(const SourceFile& sourceFile) const;
};
} // namespace cw::patina
