# Creative Workstation

A JUCE-based C++ audio workstation starter built around three layers:

- a mixing console for hands-on audio work
- a node graph for routing and modular DSP
- a functional-style DSL for programmable audio behavior

It now also includes a second executable, `Djehuti Router` (`DjeRoute`), for simple PC-wide audio patching and monitor switching.

## What’s in the starter

- Mixer-style workspace foundation
- Node graph canvas for routing and DSP
- DSL editor scaffold for functional audio scripts
- AI panel placeholder for assistant-driven patching
- MIDI control-surface support with X-Touch / Mackie-style faders and transport
- Separate `Djehuti Router` companion app for sources, sinks, and BYOK routing help
- Windows virtual driver scaffold under `Driver/` for the future OBS/Reaper device layer

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

## Release tracks

This repo publishes three separate products from one codebase, and releases are routed by product-prefixed tags:

- `creation-station-v0.1.2`
- `djehuti-router-v0.1.2`
- `djehuti-drivers-v0.1.2`

See `Release-Products.md` for the release mapping.
See `Versioning.md` for how the three products can stay version-synced when we want them to move together.

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
- Grow the MIDI layer into full learned mapping, banking, and motor-fader feedback
