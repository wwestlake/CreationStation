# Patina Surface AST Model

This page defines the first parser-facing AST for Patina surface syntax.

This is a separate model from:

- the semantic IR
- runtime lowering
- `.pta` artifact layout

That separation is intentional and important.

## Why a Separate Surface AST Exists

Patina source text contains human-oriented conveniences that should not leak directly into semantic IR.

Examples:

- import aliases
- local `let` bindings
- textual block structure
- named argument syntax
- textual type expressions

The parser should capture these faithfully in a surface AST, then a later lowering phase should normalize them into the semantic IR.

## Design Goals

The surface AST should:

- preserve source-level intent
- retain enough structure for good diagnostics
- stay simple and explicit
- be easy to lower into normalized graph form
- avoid mixing in runtime details

## Recommended Namespace

Suggested namespace:

```cpp
namespace patina
{
    namespace surface
    {
    }
}
```

This keeps the parser-facing model distinct from:

- `patina::ir`
- `patina::runtime`

## AST Layering

Recommended front-end flow:

1. tokens
2. surface AST
3. bound surface AST
4. normalized graph form
5. semantic IR

The page below focuses on step 2.

## File Model

The top-level parsed file should contain:

- optional package declaration
- optional version declaration
- imports
- graph declarations
- exports

Suggested shape:

```cpp
struct SourceFile
{
    juce::String packageName;
    juce::String version;
    juce::Array<ImportDeclaration> imports;
    juce::Array<GraphDeclaration> graphs;
    juce::Array<ExportDeclaration> exports;
};
```

This is intentionally close to what the current parser scaffolding builds.

## Source Location Model

Every AST declaration should carry source location information.

Suggested minimal form:

```cpp
struct SourceLocation
{
    int line = 0;
    int column = 0;
};
```

Eventually we may want:

- file id
- range start
- range end

But line and column are enough for v0 diagnostics.

## Qualified Names

Patina surface syntax uses dotted names for:

- node kinds
- type names
- future module/member references

Suggested shape:

```cpp
struct QualifiedName
{
    juce::StringArray segments;

    juce::String toString() const;
};
```

Examples:

- `audio.oscillator`
- `control.envelope.adsr`
- `event.note_to_frequency`

## Expression Model

The v0 surface syntax intentionally keeps expressions small.

Supported expression categories:

- identifier
- qualified name
- string literal
- integer literal
- float literal
- boolean literal

Suggested shape:

```cpp
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
```

### Why keep expressions simple?

Because v0 is trying to get:

- graph structure
- connections
- parameters
- lowering

working first.

Rich arithmetic can come later.

## Named Arguments

Node invocations use named arguments in surface syntax.

Suggested shape:

```cpp
struct NamedArgument
{
    juce::String name;
    Expression value;
};
```

This is the parser-level representation of syntax like:

```patina
audio.delay(time_ms: 280.0, feedback: 0.35)
```

## Type Expressions

Surface syntax needs a parser-friendly type representation before semantic typing happens.

Suggested shape:

```cpp
struct TypeExpression
{
    QualifiedName baseName;
    juce::StringArray genericArguments;

    juce::String toString() const;
};
```

Example:

```patina
control<f32>
```

can initially parse as:

- base name: `control`
- generic arguments: `["f32"]`

Later lowering resolves that into semantic IR typing.

## Import Declaration

Suggested shape:

```cpp
struct ImportDeclaration
{
    juce::String path;
    juce::String alias;
    SourceLocation location;
};
```

Examples:

```patina
import "@lagdaemon/core-audio"
import "@lagdaemon/modulation" as mod
```

## Let Declaration

Surface `let` declarations are sugar and should not survive unchanged into semantic IR.

Suggested shape:

```cpp
struct LetDeclaration
{
    juce::String name;
    Expression value;
    SourceLocation location;
};
```

These are resolved and inlined or translated during lowering.

## Parameter Declaration

Graphs may declare parameters.

Suggested shape:

```cpp
struct ParameterDeclaration
{
    juce::String name;
    TypeExpression type;
    bool hasDefaultValue = false;
    Expression defaultValue;
    SourceLocation location;
};
```

These are much closer to semantic meaning and should usually survive into later forms.

## Node Declaration

Node declarations represent instantiated surface-level graph components.

Suggested shape:

```cpp
struct NodeDeclaration
{
    juce::String name;
    QualifiedName callee;
    juce::Array<NamedArgument> arguments;
    SourceLocation location;
};
```

Example:

```patina
node osc = audio.oscillator(waveform: "triangle")
```

## Port References

Connection syntax uses explicit port references.

Suggested shape:

```cpp
struct PortReference
{
    juce::String nodeName;
    juce::String portName;
    SourceLocation location;
};
```

Example:

```patina
osc.out
```

## Connection Declaration

Suggested shape:

```cpp
struct ConnectionDeclaration
{
    PortReference source;
    PortReference destination;
    SourceLocation location;
};
```

Example:

```patina
connect osc.out -> amp.in
```

## Graph Declaration

A graph block groups local declarations.

Suggested shape:

```cpp
struct GraphDeclaration
{
    juce::String name;
    juce::Array<LetDeclaration> lets;
    juce::Array<ParameterDeclaration> parameters;
    juce::Array<NodeDeclaration> nodes;
    juce::Array<ConnectionDeclaration> connections;
    SourceLocation location;
};
```

### Why split statements into typed arrays?

Because for v0, the declaration categories are small and explicit. That makes:

- diagnostics easier
- lowering simpler
- editor integrations cleaner

We can always move to a generic statement list later if the language grows more complex.

## Export Declaration

Suggested shape:

```cpp
struct ExportDeclaration
{
    juce::String kind;
    juce::String graphName;
    SourceLocation location;
};
```

Supported kinds in v0:

- `instrument`
- `effect`
- `modulator`
- `graph`

## Boundaries of the Surface AST

The surface AST should preserve:

- original names
- declaration categories
- named arguments
- source-local structure

The surface AST should **not** contain:

- resolved runtime artifacts
- buffer allocation plans
- topological execution order
- LLVM lowering details
- host device handles

## What Happens After Parsing

The next stages after the surface AST should be:

### 1. Symbol binding

Resolve:

- graph-local names
- imported aliases
- duplicate declarations

### 2. Surface normalization

Normalize:

- `let` usage
- parameter defaults
- textual type forms

### 3. IR lowering

Convert:

- node declarations into semantic node instances
- connections into semantic edges
- exports into semantic graph exports

## Diagnostic Role

The surface AST should be the main anchor for parser and early binding diagnostics.

Examples:

- duplicate node name
- unknown graph in export
- malformed connection reference
- invalid type expression

This is one of the strongest reasons not to skip the surface AST and parse straight into semantic IR.

## v0 Constraints

To keep the first parser implementation commercial-grade and sane, the surface AST should deliberately avoid:

- clever implicit rewrites during parsing
- parser-side type inference
- parser-side graph execution assumptions
- expression trees more complex than needed for v0

The parser should capture what was written, not silently invent extra semantics.

## Relationship to the Current Code

The initial code scaffolding already reflects this structure in a practical v0 form:

- `SourceFile`
- `QualifiedName`
- `Expression`
- `NamedArgument`
- `TypeExpression`
- `ImportDeclaration`
- `LetDeclaration`
- `ParameterDeclaration`
- `NodeDeclaration`
- `PortReference`
- `ConnectionDeclaration`
- `GraphDeclaration`
- `ExportDeclaration`

That means the docs and the code are already converging, which is exactly what we want.

## Next Step

After this page, the next best spec is:

- `Patina Artifact Format`

That page will connect:

- semantic IR
- runtime lowering
- plugin/runtime loading
- compiled Patina artifacts
