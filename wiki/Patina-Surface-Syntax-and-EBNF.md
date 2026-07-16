# Patina Surface Syntax and EBNF

This page documents the first human-facing Patina syntax.

It is important to be precise about what this page is and is not:

- this page describes the **surface language**
- it is a projection for humans and AI prompts that want textual form
- it is **not** the semantic truth of Patina
- the semantic truth remains the Patina IR

In other words:

**Patina source text is sugar.**  
Useful sugar, powerful sugar, and worth designing carefully - but still sugar.

## Purpose of the Surface Syntax

The human-facing Patina syntax exists for four reasons:

- to let people author graphs by hand
- to give AI a stable readable target when text is useful
- to support docs, examples, and teaching
- to provide a compact editable projection of graph semantics

It should not try to expose every low-level runtime detail directly.

## Design Goals

The surface syntax should be:

- declarative first
- graph-oriented
- readable without being verbose
- musical and DSP-aware
- easy to lower into Patina IR
- easy to regenerate from IR for round-tripping

## Compiler-Grade Constraints

To keep Patina commercial-grade, the surface language should follow stricter rules from the beginning:

- lexical rules must be deterministic
- statement boundaries must be unambiguous
- block structure must be explicit
- sugar must lower predictably and reversibly
- parser convenience must never override semantic clarity

This means v0 should choose correctness and stability over clever syntax.

## Design Non-Goals

Patina surface syntax v0 is **not** trying to be:

- a general-purpose systems language
- a C++ replacement
- a bytecode definition language
- a host UI toolkit
- a low-level LLVM authoring language

## Source File Role

Suggested extension:

- `.pt`

Examples:

- `main.pt`
- `soft_keys.pt`
- `impact_chain.pt`

These files compile into a surface AST, then a normalized graph form, then semantic IR.

## Conceptual Shape

The syntax should revolve around a few main concepts:

- package
- import
- graph
- node
- connect
- export
- constants
- parameters

That means the basic authoring flow looks like:

1. declare a package or module
2. define a graph
3. declare nodes
4. connect them
5. export the graph as an instrument, effect, or modulator

## Example: Minimal Instrument

```patina
package "@lagdaemon/soft-keys"
version "0.1.0"

graph main:
    node midi   = event.midi_input()
    node notehz = event.note_to_frequency()
    node osc    = audio.oscillator(waveform: "triangle")
    node env    = control.envelope.adsr(attack: 0.05, decay: 0.12, sustain: 0.72, release: 0.28)
    node amp    = audio.gain()
    node out    = audio.output()

    connect midi.note -> notehz.note
    connect notehz.frequency -> osc.frequency
    connect osc.out -> amp.in
    connect env.out -> amp.gain
    connect amp.out -> out.in

export instrument main
```

That is the kind of shape we want:

- obvious graph structure
- readable left-to-right signal flow
- minimal punctuation burden

## Example: Effect Graph

```patina
package "@lagdaemon/shimmer-space"
version "0.1.0"

graph shimmer_fx:
    node input  = audio.input(channels: 2)
    node hp     = audio.filter.highpass(cutoff: 220.0)
    node delay  = audio.delay(time_ms: 280.0, feedback: 0.35)
    node reverb = audio.reverb(size: 0.82, mix: 0.30)
    node output = audio.output()

    connect input.out -> hp.in
    connect hp.out -> delay.in
    connect delay.out -> reverb.in
    connect reverb.out -> output.in

export effect shimmer_fx
```

## Syntax Strategy

Patina surface syntax v0 should be:

- line-oriented enough to read easily
- indentation-friendly
- not whitespace-fragile
- explicit where graph structure matters

### Block rule

For v0, a trailing `:` begins a block and the parser must require at least one following statement in that block.

### Statement rule

For v0, end-of-line is the default statement terminator.

### Indentation rule

Indentation improves readability and drives block grouping, but the parser should still rely on explicit block starters rather than trying to infer structure from whitespace alone.

## Core Constructs

### Package declaration

Declares canonical package identity.

```patina
package "@lagdaemon/soft-keys"
```

### Version declaration

```patina
version "0.1.0"
```

### Imports

Suggested forms:

```patina
import "@lagdaemon/core-audio"
import "@lagdaemon/modulation" as mod
```

### Graph declaration

```patina
graph main:
    ...
```

### Node declaration

```patina
node osc = audio.oscillator(waveform: "saw")
```

### Connection statement

```patina
connect osc.out -> amp.in
```

### Export statement

```patina
export instrument main
```

## Naming Rules

Identifiers should be simple and predictable.

Suggested rules:

- start with a letter or underscore
- continue with letters, digits, or underscores
- avoid spaces
- preserve case in metadata
- treat keywords as reserved in parser space

Examples:

- `main`
- `osc1`
- `bright_pad`
- `snare_hit`

### Why no hyphens?

Hyphens look nice in names, but they create needless ambiguity once arithmetic and other operators arrive.

For compiler sanity, v0 identifiers should not allow hyphens.

## Reserved Keywords

Initial reserved words:

- `package`
- `version`
- `import`
- `as`
- `graph`
- `node`
- `connect`
- `export`
- `instrument`
- `effect`
- `modulator`
- `let`
- `param`
- `true`
- `false`

We can extend this set later, but keeping it small helps.

## Literals

Patina v0 surface syntax should support:

- string literals
- integer literals
- floating-point literals
- boolean literals

Examples:

```patina
"triangle"
220
0.35
true
false
```

### Numeric rule

For v0:

- integer literals are base-10 only
- float literals use a decimal point
- scientific notation can be added later if needed

## Parameter Syntax

Named arguments should be the default style.

```patina
audio.oscillator(waveform: "triangle", detune: 0.15)
```

This is better than positional arguments for DSP graphs because:

- it is easier to read
- it round-trips better from structured data
- it is more stable as nodes evolve

Positional arguments should not be part of v0 surface syntax.

## Port References

Port references should be explicit.

Format:

```patina
node_name.port_name
```

Examples:

```patina
osc.out
amp.gain
notehz.frequency
```

Port references should always be explicit in connection statements. There should be no implicit default-port behavior in the textual language.

## Constants and Aliases

Suggested form:

```patina
let base_hz = 220.0
let bright = 0.72
```

Then:

```patina
node osc = audio.oscillator(frequency: base_hz)
```

This is still surface sugar and should lower to constant values in IR.

For v0, `let` bindings should be immutable and graph-local.

## Graph Parameters

Graphs should be able to expose parameters.

Suggested form:

```patina
param cutoff: control<f32> = 1200.0
param mix: control<f32> = 0.35
```

That gives a way for graphs and exported artifacts to expose a stable interface.

For v0, graph parameters should lower into named graph inputs or exported parameter metadata, not ad hoc runtime variables.

## Export Kinds

Supported export kinds in v0 surface syntax:

- `instrument`
- `effect`
- `modulator`
- `graph`

Examples:

```patina
export instrument main
export effect shimmer_fx
```

## Round-Tripping Principle

The surface syntax should be regeneratable from semantic IR.

That means:

- syntax should prefer named structure over clever shorthand
- hidden defaults should be minimized
- every meaningful node, edge, and export should have a textual equivalent

If a construct cannot reliably round-trip, it does not belong in v0 syntax sugar.

## EBNF Scope

The EBNF below is for the **surface syntax only**.

It does not describe:

- JSON manifests
- Patina IR
- `.pta` runtime artifacts
- internal lowered runtime plans

## Patina Surface EBNF v0

```ebnf
source              = { line_break } { top_level_decl { line_break } } ;

top_level_decl      = package_decl
                    | version_decl
                    | import_decl
                    | graph_decl
                    | export_decl
                    ;

package_decl        = "package" string_literal ;

version_decl        = "version" string_literal ;

import_decl         = "import" import_path [ "as" identifier ] ;

import_path         = string_literal ;

graph_decl          = "graph" identifier ":" line_break indented_graph_body ;

indented_graph_body = indent { graph_stmt line_break } dedent ;

graph_stmt          = node_decl
                    | connect_stmt
                    | let_decl
                    | param_decl
                    ;

node_decl           = "node" identifier "=" qualified_name "(" [ arg_list ] ")" ;

connect_stmt        = "connect" port_ref "->" port_ref ;

let_decl            = "let" identifier "=" expression ;

param_decl          = "param" identifier ":" type_expr [ "=" expression ] ;

export_decl         = "export" export_kind identifier ;

export_kind         = "instrument"
                    | "effect"
                    | "modulator"
                    | "graph"
                    ;

arg_list            = arg { "," arg } ;

arg                 = identifier ":" expression ;

expression          = literal
                    | identifier
                    | qualified_name
                    ;

qualified_name      = identifier { "." identifier } ;

port_ref            = identifier "." identifier ;

type_expr           = qualified_name [ "<" type_arg_list ">" ] ;

type_arg_list       = type_arg { "," type_arg } ;

type_arg            = type_expr
                    | integer_literal
                    ;

literal             = string_literal
                    | integer_literal
                    | float_literal
                    | boolean_literal
                    ;

boolean_literal     = "true" | "false" ;

identifier          = ident_start { ident_continue } ;

ident_start         = letter | "_" ;

ident_continue      = letter | digit | "_" ;
```

## Lexical Notes

The lexer should assume:

- comments begin with `#` and run to end of line
- strings are double-quoted
- line endings terminate statements
- the lexer emits `line_break`, `indent`, and `dedent` tokens
- tabs should either be forbidden or normalized consistently before lexing

### Example comments

```patina
# warm starter pad
node osc = audio.oscillator(waveform: "triangle")
```

## Parser Notes

The parser should produce a normalized surface AST first.

Recommended staged flow:

1. tokenize
2. parse source into surface AST
3. normalize names and blocks
4. bind symbols
5. resolve imports
6. lower into semantic IR

## Surface AST Direction

We should define a separate surface AST for the parser rather than parsing directly into semantic IR.

Why:

- surface syntax contains aliases and sugar
- semantic IR should stay clean and normalized
- diagnostics are easier when the parser has a surface-level model

So the chain should be:

- source text
- surface AST
- normalized graph object
- semantic IR

## Symbol Rules

The first symbol model should stay intentionally small.

### Top-level symbols

Top-level declarations may introduce:

- package identity
- imported aliases
- graph names

### Graph-local symbols

Inside a graph block, these names may be introduced:

- node names
- local `let` bindings
- graph parameters

### Shadowing

For v0:

- graph-local symbols may not shadow each other
- imported aliases may not collide with graph names
- duplicate node names are invalid

Compiler-grade tooling benefits from rejecting ambiguous naming early instead of accepting it and surprising users later.

## Lowering Boundary

The parser should not directly produce semantic IR nodes.

Instead:

- parser produces surface AST
- binder resolves names and references
- lowering phase expands sugar
- resulting normalized form maps into Patina IR

Examples of sugar that should disappear before semantic IR:

- local `let` aliases
- import aliases
- surface-only formatting conveniences

Examples of constructs that should survive into semantic IR:

- graph identity
- node identity
- typed connections
- export kind
- declared parameters

## What the EBNF Does Not Yet Cover

This v0 draft intentionally leaves some areas open:

- inline anonymous graphs
- tuple values
- arithmetic expressions beyond literals and names
- match or branch constructs
- custom event type declarations
- user-defined modules in textual syntax
- macro definitions
- generic type declarations

Those can come later once the core graph language is stable.

## Recommended Next Syntax Spec

After this page, the next useful syntax-related pages are:

- Patina surface AST model
- Patina name resolution and symbol rules
- Patina import and package resolution
- Patina diagnostics and parser error style

## Summary

Patina surface syntax should be:

- readable
- graph-centered
- declarative
- easy to lower
- easy to regenerate

And the EBNF should remain tightly scoped to the human-facing projection layer, while the semantic IR remains the true center of the system.
