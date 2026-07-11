# Creative Workstation

A JUCE-based C++ audio workstation starter built around three layers:

- a mixing console for hands-on audio work
- a node graph for routing and modular DSP
- a functional-style DSL for programmable audio behavior

## What’s in the starter

- Mixer-style workspace foundation
- Node graph canvas for routing and DSP
- DSL editor scaffold for functional audio scripts
- AI panel placeholder for assistant-driven patching

## Build

1. Set `JUCE_DIR` to the root of your local JUCE install.
2. Configure with CMake.
3. Build the generated project.

Example:

```powershell
$env:JUCE_DIR="D:\path\to\JUCE"
cmake -S . -B build
cmake --build build --config Release
```

## DSL direction

The DSL scaffold is intentionally small right now, but it points toward:

- functional composition with `let`, `fn`, `graph`, `route`, and `emit`
- compile-time validation before audio execution
- later stages for type checking, lowering, and JIT or offline compilation
- AI-assisted patch and script generation

## Next ideas

- Add real audio routing and clip transport
- Add project and session persistence
- Expand the DSL into a parser, type checker, and execution backend
- Connect AI to patch generation and code synthesis
