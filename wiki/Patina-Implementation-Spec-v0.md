# Patina Implementation Spec v0

This page turns the broader Patina architecture into the first concrete implementation target.

The goal is to define the smallest serious version of the platform we can actually build against inside Creation Station.

This spec covers:

- semantic IR building blocks
- audio specialization types
- runtime artifact shape
- project and package manifests
- optimization profiles
- execution domains
- validation rules

This is **v0**, which means it is intentionally narrow enough to implement while still being architecturally honest.

## Scope of v0

Patina v0 is focused on **audio only**.

It does not try to solve animation, simulation, gameplay, or general UI logic yet.

It should be able to represent:

- instruments
- effects
- modulators
- automation
- routing
- signal analysis helpers

It should be usable for:

- Signal Lab patch export
- runtime plugin loading
- graph-driven effect chains
- AI-generated structured patch definitions

## Core Design Rule

The **semantic IR** is the truth.

Everything else lowers into it:

- human Patina source
- JSON graph objects
- node editor graphs
- AI-generated structures

## Semantic IR v0

Patina IR v0 is a typed graph model.

### Primitive concepts

Every IR graph is composed from:

- **types**
- **ports**
- **nodes**
- **edges**
- **resources**
- **graphs**
- **exports**

### Type categories

Patina v0 should support these type families:

#### Scalar values

- `bool`
- `i32`
- `i64`
- `f32`
- `f64`
- `string` for metadata only, not audio-thread use

#### Audio and control streams

- `audio<f32,1>`
- `audio<f32,2>`
- `control<f32>`
- `control<bool>`

#### Events

- `trigger`
- `event<midi>`
- `event<note>`
- `event<transport>`

#### Resources

- `buffer<f32>`
- `sample_asset`
- `table<f32>`

### Node model

Each node should include:

- stable `id`
- `kind`
- `domain`
- typed input ports
- typed output ports
- optional parameter block
- effect metadata

Suggested shape:

```json
{
  "id": "osc1",
  "kind": "audio.oscillator",
  "domain": "audio",
  "inputs": [],
  "outputs": [
    { "name": "out", "type": "audio<f32,1>" }
  ],
  "params": {
    "waveform": "saw",
    "frequency": 220.0
  },
  "effects": ["realtime-safe"]
}
```

### Edge model

Edges connect one typed port to another typed port.

Suggested rules:

- exact type match by default
- explicit conversion nodes required when types differ
- graph must be acyclic in audio/control execution sections for v0
- event scheduling edges may cross time without becoming raw feedback loops

### Graph model

Patina v0 graphs should contain:

- graph identity
- node set
- edge set
- imported resources
- exported entry points
- metadata

## Audio Specialization v0

Patina Audio v0 should support a practical starter set of node kinds.

### Source nodes

- `audio.oscillator`
- `audio.noise`
- `audio.sample_player`
- `audio.input`
- `event.midi_input`

### Shaping nodes

- `control.envelope.adsr`
- `control.lfo`
- `control.curve`
- `control.constant`
- `control.scale`

### DSP nodes

- `audio.gain`
- `audio.pan`
- `audio.mix`
- `audio.filter.lowpass`
- `audio.filter.highpass`
- `audio.saturator`
- `audio.delay`
- `audio.reverb`

### Analysis nodes

- `audio.scope_tap`
- `audio.spectrum_tap`
- `audio.level_meter`

### Routing and utility nodes

- `audio.output`
- `audio.channel_split`
- `audio.channel_merge`
- `event.note_to_frequency`
- `control.map_range`

## Execution Domains

Patina v0 should define four execution domains.

### `audio`

Hard real-time domain.

Rules:

- no allocation
- no blocking
- no filesystem
- no network
- no unpredictable host calls

### `control`

Low-rate modulation and parameter update domain.

Allowed:

- precomputed automation
- mapped controller state
- envelope stepping

### `event`

Discrete event processing for:

- MIDI
- transport
- triggers
- note lifecycle

### `worker`

Deferred or non-real-time operations:

- file loading
- analysis
- package resolution
- remote content access

## Effect Metadata

Every node in v0 should be taggable with execution behavior.

Suggested tags:

- `realtime-safe`
- `deferred`
- `io-bound`
- `debug-only`

The validator should reject placement of non-`realtime-safe` nodes in audio execution segments.

## Validation Rules v0

The validator should enforce:

### Graph integrity

- no duplicate node ids
- no missing referenced ports
- no missing node references in edges

### Type correctness

- input and output types must match
- implicit conversion is not allowed in v0

### Domain correctness

- forbidden domain crossings require explicit bridge nodes
- worker-domain nodes cannot sit directly in audio chains

### Real-time safety

- audio-path nodes must all be `realtime-safe`

### Entry-point correctness

- exported instrument/effect graphs must provide valid declared outputs

## Runtime Artifact v0

Patina runtime artifacts should use the `.pta` extension.

The `.pta` file is the loadable runtime package used by Creation Station or plugins.

### Artifact sections

Patina artifact v0 should include:

- header
- metadata
- semantic graph hash
- lowered runtime graph
- dependency list
- export table
- optional debug table

### Header fields

Suggested header metadata:

- artifact version
- runtime ABI version
- target platform
- target architecture
- build profile
- entry kind

### Entry kinds

Supported entry kinds in v0:

- `instrument`
- `effect`
- `modulator`
- `graph`

### Example artifact metadata

```json
{
  "artifactVersion": "0.1",
  "abiVersion": "0.1",
  "entryKind": "instrument",
  "buildProfile": "interactive",
  "platform": "windows-x64",
  "exports": ["main"]
}
```

## Project Manifest v0

Project root file:

- `patina-project.json`

Suggested shape:

```json
{
  "schemaVersion": "0.1",
  "name": "soft-keys-lab",
  "displayName": "Soft Keys Lab",
  "version": "0.1.0",
  "package": "@lagdaemon/soft-keys-lab",
  "runtime": {
    "patina": "0.1",
    "abi": "0.1"
  },
  "build": {
    "profile": "interactive",
    "target": "instrument",
    "entryGraph": "graphs/main.json",
    "output": "build/soft-keys-lab.pta"
  },
  "dependencies": {
    "@lagdaemon/core-audio": "^0.1.0"
  }
}
```

### Required fields

- `schemaVersion`
- `name`
- `version`
- `runtime`
- `build`

### Build target values

Patina v0 supports:

- `instrument`
- `effect`
- `modulator`
- `graph`

## Package Manifest v0

Package manifest file:

- `patina-package.json`

Suggested shape:

```json
{
  "schemaVersion": "0.1",
  "name": "@lagdaemon/core-audio",
  "version": "0.1.0",
  "kind": "library",
  "patina": {
    "runtime": "0.1",
    "abi": "0.1"
  },
  "exports": {
    "graphs": [
      "graphs/basic-subtractive.json"
    ],
    "modules": [
      "modules/lowpass.json",
      "modules/envelope-adsr.json"
    ]
  },
  "dependencies": {},
  "platforms": [
    "windows-x64"
  ]
}
```

### Package kinds

Supported kinds in v0:

- `library`
- `instrument-pack`
- `effect-pack`
- `content-pack`

## Lock File v0

Lock file:

- `patina-lock.json`

Purpose:

- freeze dependency resolution
- record exact package versions
- record artifact hashes
- lock runtime compatibility assumptions

Suggested shape:

```json
{
  "schemaVersion": "0.1",
  "packages": {
    "@lagdaemon/core-audio": {
      "version": "0.1.0",
      "source": "registry",
      "hash": "sha256:abc123"
    }
  }
}
```

## Optimization Profiles v0

Patina v0 should support these build profiles.

### `debug`

Priorities:

- maximum diagnostics
- no aggressive graph rewriting
- preserve mapping to source and graph ids

### `interactive`

Priorities:

- quick compile
- low startup cost
- good enough optimization for live design work

### `realtime`

Priorities:

- lower jitter
- tighter scheduling
- allocation-free lowered plan

### `render`

Priorities:

- heavier optimization
- quality and throughput over startup latency

## Optimization Passes v0

The first pass pipeline should include:

- constant folding
- dead node elimination
- unused export trimming
- graph normalization
- constant parameter inlining
- channel specialization
- simple buffer reuse planning

LLVM-specific lowering can come after this stage rather than replacing it.

## Interpreter Runtime v0

The first runtime backend should be an interpreter.

It should:

- load semantic graph data
- validate graph structure
- build an execution plan
- step event/control domains
- process audio blocks

This gives us a reference runtime before deeper LLVM work.

## LLVM JIT Boundary v0

LLVM should not be the language.

LLVM enters after:

- semantic validation
- initial optimization
- lowered runtime planning

The first JIT slice should probably target:

- oscillator kernels
- filter kernels
- small voice chains
- specialized instrument graphs

That keeps JIT focused on the hot path instead of the whole world.

## Relationship to Creation Station v0

The first practical Patina integrations inside Creation Station should be:

- Signal Lab patch export to Patina graph form
- plugin runtime loading of `.pta` instrument artifacts
- graph panel as a front end to Patina Audio structures
- AI assistant generating starter graph objects

## First Build Milestone

The first build milestone should deliver:

1. `patina-project.json` schema
2. `patina-package.json` schema
3. semantic IR C++ model
4. validator
5. interpreter runtime
6. `.pta` artifact writer/loader
7. one instrument graph end-to-end

If that works, then Patina stops being an idea and becomes a real platform.

## Next Spec Pages

After this page, the next useful specs are:

- Patina IR C++ type model
- Patina artifact binary/container format
- Patina Audio node catalog
- Alloy command surface
- human syntax sugar draft
