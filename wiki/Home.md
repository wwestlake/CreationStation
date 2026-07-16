# CreationStation

Welcome to the CreationStation wiki.

CreationStation is a creative audio workstation for making, shaping, and discovering sound.
It starts as a DAW, but the real goal is bigger: a place where mixing, modular DSP, code,
and AI-assisted idea generation all live in one flow.

## Direction

The software is being built around a few core beliefs:

- music creation should feel immediate, hands-on, and exploratory
- the technical layer should be powerful, but never trap the creative layer
- a functional DSP language can unlock ideas that canned effects cannot
- node graphs, mixer workflows, and code should all point at the same engine
- AI should help sketch, explain, and generate ideas without taking the wheel

## What We Are Building

The first version is a working creative studio with:

- a standard DAW-style mixer and transport
- a node graph for routing and modular processing
- a serious DSP language that can evolve into a compiler or JIT backend
- MIDI control-surface support, especially X-Touch
- an AI assistant that helps write patches, language code, and creative prompts

## Design Philosophy

CreationStation should feel like a hybrid of a DAW, a visual synth lab, and a programmable
instrument. The interface should invite play and experimentation, not just engineering.

That means:

- usable before it is complete
- creative before it is rigid
- open to performance, live tweaking, and rapid iteration
- structured enough to scale into a serious tool

## Current Shape

The current codebase already has:

- a JUCE C++ desktop app shell
- a tabbed workspace for mixer, graph, DSL, and AI views
- a demo audio engine that produces sound
- a first MIDI control-surface layer for X-Touch-style hardware
- a starter DSL compiler scaffold

## Next Coding Steps

The next useful slices are likely:

1. real channel meters and master bus behavior
2. banked X-Touch mapping with fader feedback
3. editable node graph routing
4. DSL parsing and AST construction
5. audio graph execution and hot reload
6. AI-assisted patch generation

## Project Links

- Repository: https://github.com/wwestlake/CreationStation
- Project: https://github.com/users/wwestlake/projects/17

## Current Product Direction

Creation Station is now leaning harder into creative sound design:

- Foley staging and clip shaping
- Signal Lab for procedural sound creation
- automation-driven sound motion
- exportable patch logic
- runtime plugin integration for DAWs like Reaper

## Runtime Plugin Direction

The long-term ecosystem is bigger than the standalone app.

Creation Station will author patches that can be loaded by focused runtime plugins:

- [Plugin Family Architecture](Plugin-Family-Architecture.md)
- [Patch Format and Export](Patch-Format-and-Export.md)
- [Patch Schema Draft](Patch-Schema-Draft.md)

## Patina Language Direction

Creation Station is also becoming the proving ground for a deeper language and runtime platform:

- [Patina Core and Audio Architecture](Patina-Core-and-Audio-Architecture.md)
- [Patina Implementation Spec v0](Patina-Implementation-Spec-v0.md)
- [Patina IR C++ Type Model](Patina-IR-Cpp-Type-Model.md)
- [Patina Surface Syntax and EBNF](Patina-Surface-Syntax-and-EBNF.md)

## Cross-Platform Notes

- [Linux Port Notes](Linux-Port-Notes.md)

## Legacy Notes

Earlier router/driver exploration is still documented here for reference only:

- [Router Device Layer](Router-Device-Layer.md)
- [Virtual Driver Path](Virtual-Driver-Path.md)
