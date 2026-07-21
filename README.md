# Creation Station

A JUCE-based C++ creative audio tool focused on sound design, effects, and programmable audio workflows.

## What’s in the starter

- Mixer-style workspace foundation
- Arrange view for clips and sketching ideas
- Node graph canvas for routing and DSP
- DSL editor scaffold for functional audio scripts
- AI panel placeholder for assistant-driven patching
- MIDI control-surface support with X-Touch / Mackie-style faders and transport
- VST3 hosting groundwork for building sounds with plugins

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

## Release tags

This repo publishes `Creation Station` with product-prefixed tags such as:

- `creation-station-v0.1.2`

## DSL direction

The DSL scaffold is intentionally small right now, but it points toward:

- functional composition with `let`, `fn`, `graph`, `route`, and `emit`
- compile-time validation before audio execution
- later stages for type checking, lowering, and JIT or offline compilation
- AI-assisted patch and script generation

## Next ideas

- Grow the app into a focused sound-design workstation
- Add project and session persistence
- Expand the DSL into a parser, type checker, and execution backend
- Connect AI to patch generation and code synthesis
- Deepen plugin workflows for creating layered effects and textures
