# Patina IR C++ Type Model

This page defines the first C++ model for Patina's semantic IR.

The purpose of this page is not to lock down every implementation detail forever. It is to establish the first durable shape of the in-memory model that Creation Station, the Patina validator, the interpreter runtime, and later the LLVM backend can all agree on.

This is the **typed semantic model**, not the surface syntax model.

## Goals

The C++ IR model should:

- represent the semantic truth of Patina graphs
- be stable enough for serialization and validation
- support audio-first execution in v0
- keep room for future non-audio domain specialization
- cleanly separate graph meaning from runtime lowering

## Design Rules

### 1. Semantic before executable

The IR model represents *what the graph means*, not yet *how it runs*.

That means:

- semantic nodes are not DSP kernels
- types are not raw buffer pointers
- edges are not yet scheduled instructions

### 2. Strong identity everywhere

Every important object should have a stable identifier.

That includes:

- graph ids
- node ids
- port ids
- resource ids
- export ids

### 3. Explicit typing

Ports and resources must carry explicit type information.

No hidden type inference should be required to understand a graph once it is in semantic IR form.

### 4. Domain awareness

Every node and port should know its execution domain or inherit a clear default.

### 5. Metadata is separate from semantics

Nice labels, UI hints, debug names, and documentation are important, but they should not be mixed into the fields required for execution meaning.

## Suggested Namespace Layout

Suggested C++ namespace:

```cpp
namespace patina
{
    namespace ir
    {
    }
}
```

This keeps the semantic model distinct from:

- parser code
- runtime code
- host integration
- UI/editor code

## Core Enums

The semantic model should begin with a small set of explicit enums.

### `Domain`

```cpp
enum class Domain
{
    audio,
    control,
    event,
    worker,
    ui
};
```

### `TypeKind`

```cpp
enum class TypeKind
{
    scalar,
    stream,
    event,
    resource,
    module,
    tuple
};
```

### `ScalarKind`

```cpp
enum class ScalarKind
{
    boolean,
    i32,
    i64,
    f32,
    f64,
    string
};
```

### `StreamKind`

```cpp
enum class StreamKind
{
    audio,
    control
};
```

### `EventKind`

```cpp
enum class EventKind
{
    trigger,
    midi,
    note,
    transport,
    custom
};
```

### `ResourceKind`

```cpp
enum class ResourceKind
{
    buffer,
    sampleAsset,
    table,
    externalInput,
    externalOutput
};
```

### `NodeEffectFlag`

```cpp
enum class NodeEffectFlag : uint32_t
{
    none          = 0,
    realtimeSafe  = 1 << 0,
    deferred      = 1 << 1,
    ioBound       = 1 << 2,
    debugOnly     = 1 << 3
};
```

### `PortDirection`

```cpp
enum class PortDirection
{
    input,
    output
};
```

## Stable Identifier Types

Avoid raw strings everywhere in internal code once parsed.

Suggested wrappers:

```cpp
struct GraphId   { juce::String value; };
struct NodeId    { juce::String value; };
struct PortId    { juce::String value; };
struct ResourceId{ juce::String value; };
struct ExportId  { juce::String value; };
struct TypeId    { juce::String value; };
```

These can still serialize to plain strings, but the wrappers make misuse harder in code.

## Type Model

The type model should use tagged structs rather than giant string-based type blobs.

## `Type`

```cpp
struct Type
{
    TypeKind kind = TypeKind::scalar;

    ScalarKind scalarKind = ScalarKind::f32;
    StreamKind streamKind = StreamKind::audio;
    EventKind eventKind = EventKind::trigger;
    ResourceKind resourceKind = ResourceKind::buffer;

    int channelCount = 1;
    juce::String customEventName;
    juce::Array<Type> tupleElements;
};
```

### Notes

- `channelCount` applies mainly to stream types
- `customEventName` only matters for `EventKind::custom`
- `tupleElements` is only used when `kind == TypeKind::tuple`

This is intentionally compact for v0.

## Constant Value Model

Patina IR will need literal values for parameters, defaults, and small constant nodes.

Suggested model:

```cpp
using ConstantValue = std::variant<bool, int32_t, int64_t, float, double, juce::String>;
```

If variant use becomes awkward with current toolchain constraints, we can wrap it:

```cpp
struct Constant
{
    ScalarKind kind = ScalarKind::f32;
    ConstantValue value;
};
```

## Parameter Model

Parameters are semantic inputs to nodes or exported modules.

```cpp
struct Parameter
{
    juce::String id;
    juce::String displayName;
    Type type;
    std::optional<Constant> defaultValue;
    std::optional<Constant> minValue;
    std::optional<Constant> maxValue;
    juce::String unit;
};
```

## Port Model

Ports must be strongly typed and directionally explicit.

```cpp
struct Port
{
    PortId id;
    juce::String name;
    PortDirection direction = PortDirection::input;
    Type type;
    Domain domain = Domain::audio;
    bool required = true;
};
```

### Notes

- `name` is what humans see
- `id` is what the graph references
- `domain` is repeated here because future mixed-domain nodes may expose ports in different domains

## Resource Reference Model

Resources represent external data or host-bound objects the graph depends on.

```cpp
struct ResourceRef
{
    ResourceId id;
    juce::String name;
    Type type;
    ResourceKind kind = ResourceKind::buffer;
    juce::String sourcePath;
    juce::NamedValueSet metadata;
};
```

Examples:

- sample asset file
- wavetable
- external audio input endpoint
- named output sink

## Node Model

Nodes are the heart of the semantic graph.

```cpp
struct Node
{
    NodeId id;
    juce::String kind;
    Domain domain = Domain::audio;

    juce::Array<Port> inputs;
    juce::Array<Port> outputs;
    juce::Array<Parameter> parameters;

    juce::NamedValueSet properties;
    uint32_t effectFlags = static_cast<uint32_t>(NodeEffectFlag::none);

    juce::String displayName;
    juce::String description;
};
```

### Field meaning

- `kind` is the semantic operator class, such as `audio.oscillator`
- `parameters` are formal semantic parameters
- `properties` hold configured values or small semantic annotations
- `effectFlags` are runtime-safety and execution hints

### Why both `parameters` and `properties`?

Because they mean different things:

- `parameters` describe the allowed shape of input configuration
- `properties` hold the configured semantic state of this node instance

That keeps node definitions cleaner.

## Edge Model

Edges connect a source port to a destination port.

```cpp
struct PortRef
{
    NodeId nodeId;
    PortId portId;
};

struct Edge
{
    juce::String id;
    PortRef source;
    PortRef destination;
    juce::NamedValueSet metadata;
};
```

### Rules

For v0:

- one output port may feed multiple downstream inputs
- input ports accept at most one incoming edge unless the node kind explicitly allows fan-in through prior lowering
- type compatibility is checked at validation time

## Export Model

Exports define the public entry surface of a graph artifact.

```cpp
enum class ExportKind
{
    instrument,
    effect,
    modulator,
    graph
};

struct Export
{
    ExportId id;
    ExportKind kind = ExportKind::graph;
    juce::String name;
    GraphId graphId;
    juce::Array<PortRef> entryPorts;
    juce::Array<PortRef> exitPorts;
};
```

### Notes

For v0, a `.pta` artifact can expose a primary export named `main`, but the model should already support multiple exports.

## Graph Model

Each Patina graph is a semantic unit with nodes, edges, resources, and exports.

```cpp
struct Graph
{
    GraphId id;
    juce::String name;
    juce::String description;

    juce::Array<Node> nodes;
    juce::Array<Edge> edges;
    juce::Array<ResourceRef> resources;
    juce::Array<Export> exports;

    juce::NamedValueSet metadata;
};
```

### Metadata examples

- source patch id
- source package name
- author
- creation timestamp
- debug labels

## Module Model

Packages may export modules made of one or more graphs.

```cpp
struct Module
{
    juce::String id;
    juce::String name;
    juce::String version;
    juce::Array<Graph> graphs;
    juce::NamedValueSet metadata;
};
```

This allows the semantic model to describe:

- a single instrument graph
- a family of helper graphs
- reusable building blocks in one module package

## Whole IR Document Model

At the top level, the semantic IR document should gather everything needed for validation and artifact emission.

```cpp
struct Document
{
    juce::String schemaVersion;
    juce::String runtimeVersion;
    juce::String abiVersion;

    juce::String packageName;
    juce::String packageVersion;

    juce::Array<Module> modules;
    juce::NamedValueSet metadata;
};
```

This `Document` is the semantic payload that eventually gets:

- validated
- optimized
- lowered
- serialized to `.pta`

## Semantic vs Lowered Models

We should not force one struct set to do both jobs.

Recommended split:

- `patina::ir::*` = semantic truth
- `patina::runtime::*` = lowered execution plan

Examples of things that belong in runtime lowering, not semantic IR:

- topological execution order
- buffer slot allocation
- block specialization plans
- SIMD packing decisions
- JIT handles

## Validation Helpers

The C++ model should support clean validation queries.

Useful helper functions:

```cpp
const Node* findNode(const Graph&, const NodeId&);
const Port* findPort(const Node&, const PortId&);
bool isRealtimeSafe(const Node&);
bool isCompatible(const Type&, const Type&);
```

The validator should use these helpers rather than spreading lookup logic everywhere.

## Serialization Strategy

The C++ model should serialize cleanly to JSON in v0.

That means:

- enums need stable string forms
- ids serialize as plain strings
- `NamedValueSet` use should be limited to small metadata/property bags
- core semantic fields should always use explicit named members

This matters because AI, node editing, manifests, and package exchange will all lean on JSON early.

## Example Node in C++ Terms

An oscillator instance might look like:

```cpp
Node oscillator
{
    NodeId{ "osc1" },
    "audio.oscillator",
    Domain::audio,
    {},
    {
        Port{ PortId{ "out" }, "out", PortDirection::output, Type{ TypeKind::stream, ScalarKind::f32, StreamKind::audio, EventKind::trigger, ResourceKind::buffer, 1 }, Domain::audio, true }
    },
    {},
    {},
    static_cast<uint32_t>(NodeEffectFlag::realtimeSafe),
    "Main Oscillator",
    "Primary tone source"
};
```

The exact constructor style can change, but the shape should remain consistent.

## v0 Constraints

To keep implementation practical, the first C++ IR model should deliberately avoid:

- arbitrary recursive type algebra
- dynamic polymorphic node subclasses
- graph mutation hidden behind side-effect-heavy editor code
- host-specific runtime handles inside semantic structs

The model should stay plain, explicit, and serializable.

## Recommended File Layout

Suggested future code layout:

```text
Source/Patina/IR/
  PatinaTypes.h
  PatinaIds.h
  PatinaConstants.h
  PatinaPorts.h
  PatinaNodes.h
  PatinaGraph.h
  PatinaDocument.h
  PatinaValidation.h
```

We can collapse this into fewer files at first, but the conceptual split is useful.

## First C++ Build Target

The first code milestone based on this page should produce:

1. C++ headers for the IR structs
2. enum-to-string helpers
3. JSON serialization helpers
4. a validator that can load and check a simple instrument graph
5. one round-trip test from graph JSON to IR to JSON

Once that exists, Patina has a real semantic backbone in code.

## Next Step

After this page, the next best spec is:

- `Patina Artifact Format`

That will define how the semantic IR and lowered runtime forms are packaged for loading by Creation Station and the runtime plugins.
