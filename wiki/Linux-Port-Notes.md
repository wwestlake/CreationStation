# Linux Port Notes

This page is for the Linux Creation Station effort so the Windows and Linux tracks stay aligned on product behavior, even where the implementation differs.

## Purpose

The Windows codebase is currently the clearest expression of the product direction for:

- creative workspaces
- Signal Lab
- patch export/import
- content library behavior
- LagDaemon login and entitlement-driven content access

Linux does **not** need to copy Windows implementation details where the platform offers a better path. The goal is behavioral alignment, not forced technical symmetry.

## Current Product Shape

Creation Station is no longer being treated primarily as a traditional DAW clone.

The current direction is:

- **Foley** for arranging and shaping recorded sound events
- **Signal** for procedural sound design and synthesis experiments
- **Library** for downloaded, premium, and local content
- **Layers** for mixer-style balancing
- **Patch** for patch logic and future modular workflows
- **Script** for future programmable DSP tooling
- **Capture** for recording flows
- **AI** for assistant-guided workflows

These workspace names are visible in the Windows app and should be treated as the current product vocabulary unless Linux has a strong reason to diverge.

## Major Windows Features Implemented

### 1. Storage Root Selection

The app does **not** assume `Documents`, cloud storage, or a fixed OS user folder for content/projects.

Behavior:

- on first use, user chooses a local storage root
- app stores a small pointer file near the executable
- project/config/content folders live under the chosen root

Current subfolders under the chosen storage root:

- `Projects`
- `Config`
- `Content/BuiltIn`
- `Content/Downloaded`
- `Content/User`

Linux should preserve this philosophy:

- let the user choose where the app stores its working data
- do not silently dump all project/content data into a default personal folder unless Linux explicitly decides to offer that as an option

### 2. Signal Lab

Signal Lab is the strongest current creative feature and should be treated as a key parity target.

Implemented behavior on Windows:

- oscillator mix
  - sine
  - saw
  - square
  - triangle
  - noise
- duration / base frequency / pitch sweep controls
- envelope editing
- oscilloscope display
- frequency analyzer display
- automation lanes
  - pitch motion
  - gain motion
- preview generation
- render/export into project assets

Linux should aim to preserve the user-facing workflow even if rendering or visualization internals differ.

### 3. Patch Format + Runtime

Windows now has a shared patch model and runtime player.

Implemented concepts:

- `PatchDocument`
- parameters
- automation lanes
- sources
- nodes
- connections
- output
- JSON serialization / parsing
- export/import of `.cspatch`

Current runtime support is still intentionally narrow:

- instrument-style procedural patches
- oscillator/noise sources
- envelope node
- pitch automation
- gain automation
- output gain

Relevant Windows files:

- `Source/Patch/PatchModel.h`
- `Source/Patch/PatchModel.cpp`
- `Source/Audio/PatchRuntimePlayer.h`
- `Source/Audio/PatchRuntimePlayer.cpp`

Linux should read these files as the behavioral reference for patch schema and minimal runtime expectations.

### 4. Foley / Arrange Flow

Windows currently supports a Foley-style arrangement workflow rather than a full conventional DAW timeline.

Implemented ideas:

- import project sounds
- place clips into arrangement lanes
- duplicate/delete selected clip
- clip inspector for trim/gain/fades/reverse/normalize
- engine playback for arranged clips

This should help the Linux side decide whether to preserve the same creative-first framing instead of defaulting back to a generic track recorder.

### 5. Content Library

The app now has a real content library direction tied to LagDaemon.

Local content origins:

- built-in
- downloaded
- user

Remote content origin:

- LagDaemon service

Current Windows behavior:

- scans local storage content folders
- shows local library items
- signs into LagDaemon
- fetches remote library for `creation-station`
- merges remote items with local items
- hides remote duplicates once content is downloaded locally
- allows download of available remote items
- allows reveal/open-folder for local items

Relevant Windows files:

- `Source/Content/ContentLibrary.h`
- `Source/Content/ContentLibrary.cpp`
- `Source/Content/ContentApiClient.h`
- `Source/Content/ContentApiClient.cpp`
- `Source/Views/ContentPanel.h`
- `Source/Views/ContentPanel.cpp`

## LagDaemon Auth + Content Contract

Windows already supports desktop sign-in against LagDaemon using the loopback browser flow.

Key points:

- app signs in through LagDaemon site
- receives JWT
- JWT contains:
  - role
  - entitlements
- admin UI is shown based on JWT role
- content library is filtered by product slug

Current production base for API calls:

- `https://lagdaemon.com/djehuti`

Current product slug used by the Windows app:

- `creation-station`

Important content endpoints already in use:

- `GET /api/content/library?product={slug}`
- `POST /api/content/{id}/download`
- admin publishing endpoints for content creation/upload

Linux should follow the same API contract unless the website team changes it.

## Admin Content Publishing

Windows now supports admin publishing from inside the app.

Current behavior:

- admin-only `Admin Publish` button in Library
- choose a package file
- enter metadata
- create content record on LagDaemon
- upload package bytes
- refresh library

This is important because content is now part of the product ecosystem, not just local files.

## Plugin / Ecosystem Direction

The longer-term direction is not only the standalone app.

Windows wiki pages already documenting that direction:

- `Plugin-Family-Architecture.md`
- `Patch-Format-and-Export.md`
- `Patch-Schema-Draft.md`

Linux should read those as product-direction documents, not just Windows implementation notes.

## What Linux Should Probably Prioritize Next

Recommended parity order:

1. workspace vocabulary and layout alignment
2. chosen storage root behavior
3. Signal Lab workflow parity
4. `.cspatch` compatibility
5. content library + LagDaemon sign-in
6. downloaded content install/use flow

That order keeps Linux aligned with the current product identity instead of spending effort on old router/driver explorations that are no longer central to Creation Station.

## What Not To Mirror Blindly

Linux should **not** copy Windows just because Windows did it first.

Examples where Linux can do better:

- audio device plumbing
- file dialogs
- path conventions
- packaging
- plugin hosting specifics
- low-level audio backend choices

If Linux has a simpler or more natural platform-native path, use it — just keep the product behavior and patch/content compatibility aligned.

## Important Product Callouts

- Router/virtual-driver exploration was spun down on the Windows side and is no longer the main product direction for this repo
- Creation Station is now focused on creative sound design, content, patching, and assistant-driven workflows
- LagDaemon-backed content is a first-class feature, including free and tier-gated items
- The app should respect user control over local storage

## Suggested Read Order For Linux Dev

1. `wiki/Home.md`
2. `wiki/Patch-Format-and-Export.md`
3. `wiki/Patch-Schema-Draft.md`
4. `wiki/Plugin-Family-Architecture.md`
5. Windows source:
   - `Source/Views/SignalLabPanel.*`
   - `Source/Patch/PatchModel.*`
   - `Source/Audio/PatchRuntimePlayer.*`
   - `Source/Content/ContentLibrary.*`
   - `Source/Content/ContentApiClient.*`
   - `Source/MainComponent.*`

## Bottom Line

For Linux, the best short summary is:

**Port the product behavior, not the Windows baggage.**

The most important things to preserve are:

- creative workspace model
- Signal Lab
- patch compatibility
- LagDaemon auth/content integration
- user-controlled local storage
