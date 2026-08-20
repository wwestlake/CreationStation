# Creation Shared Language Rollout

This plan aligns the four Creation apps around one language family with per-app host boundaries:

- Creation Station — audio / patch / tracker / performance
- Creation Engine — world / gameplay / simulation / server
- Creation Movie — timeline / media / render
- Creation Live — scenes / broadcast / cues / streaming

## Core Direction

Use one shared language frontend and runtime strategy across all apps:

- one syntax family
- one compiler pipeline
- one LLVM/JIT strategy on Windows
- per-app host intrinsics and domain allow-lists so code cannot run in the wrong app

See also:

- [Creation Suite Interop Specification](./Creation-Suite-Interop-Spec.md)

## Windows LLVM Strategy

Adopt Creation Engine's current pattern as the baseline for all apps:

1. Visual Studio 17 2022 only
2. same JUCE checkout: `D:\JUCE2\JUCE`
3. additive `CMAKE_PREFIX_PATH` discovery, not first-configure-only toolchain-file dependency
4. local `vcpkg_installed/x64-windows` first
5. explicit override via `-DCE_LLVM_VCPKG_DIR=<path>` when no local/relative vcpkg install is found (no hardcoded developer-machine path)

This avoids repeated multi-hour LLVM rebuilds while still letting each repo move to its own manifest-local install later.

## App Boundaries

Each app should expose a language host policy layer that answers:

- what app domain am I
- which node/code domains are allowed here
- which intrinsics are exported here
- which runtime services exist here

That policy should be data-driven eventually, but a small hard gate per app is enough for the first pass.

## Creation Station Integration Plan

Resolved: the bespoke `Patina` DSL (lexer/parser/lowering/artifact format) has been removed outright. Station consumes the shared CEL frontend directly — no app-local language implementation, no host-dialect decision to make. Remaining steps:

1. add the same shared LLVM discovery helper used by Creation Engine/Movie/Live
2. introduce a `Creation Station` language host policy layer beside the current language code
3. classify Station-safe domains:
   `shared`, `audio`, `patch`, `tracker`, `mixer`, `performance`
4. explicitly reject non-Station domains:
   `world`, `physics`, `broadcast`, `scene`, `render`
5. separate frontend/runtime-neutral language pieces from Station-only audio host intrinsics

## Recommended Sequencing

1. keep Creation Engine as the reference LLVM host until the shared helper stabilizes
2. scaffold Movie and Live with the same discovery pattern and domain-policy layer
3. add the shared LLVM helper to Creation Station
4. define the cross-app domain registry and intrinsic ownership map
5. only then start consolidating compiler/runtime code

The asset/version/reference side of that contract now lives in:

- [Creation Suite Interop Specification](./Creation-Suite-Interop-Spec.md)

## Non-Goal For The First Pass

Do not try to force all four apps onto one giant repo or one giant runtime immediately. First align:

- build environment
- LLVM discovery
- app-domain policy
- naming and folder conventions

Then merge deeper language/runtime pieces intentionally instead of by accident.
