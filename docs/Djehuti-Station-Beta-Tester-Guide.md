# Djehuti Station — Beta Tester Guide

This guide is written from the actual current source code and the project's own tracking docs (not aspirational feature lists), so you know exactly what to expect, what to try, and what not to be surprised by. If something here turns out to be wrong once you're in the app, that's useful feedback in itself — the app is moving fast.

## What Djehuti Station Is

Djehuti Station is a JUCE-based audio creative workstation: multi-track recording and arrangement, VST3 plugin hosting, MIDI control-surface support, and an experimental visual node-graph sound-design layer (Signal Lab) built on FRust, a shared scripting language used across the whole Djehuti/Creation Suite product line.

You may notice the app still says "Creation Station" in a few internal places — install paths, some file names, a bundle ID. That's leftover from a recent rebrand to "Djehuti Station" and is harmless; the visible app name, window titles, and product identity are all "Djehuti Station."

## Before You Start

- This is a beta build under active development, not a finished product. Some tools you'll see are real and working end-to-end; others are genuinely present in the UI but not yet wired to do anything audible. This guide tells you which is which — please don't file "it doesn't do anything" reports on the ones explicitly called out below as not-yet-wired; do tell us if something *documented as working* doesn't work for you.
- Save often. Project save/load is solid in testing, but this is still beta software — treat your work as safe-to-lose until you've confirmed round-tripping matters to you.
- You'll be asked to choose a storage location (the "suite VFS root") the first time you configure things — this is a real, one-time folder choice, not a bug.

## Core Workflows — These Work End-to-End

### 1. Start a project, record and arrange audio, save it

The most solid, well-tested loop in the app.

1. Create a new project (Project menu → New Project). Choose your storage location if this is your first project.
2. Add one or more tracks (audio, MIDI, or other track kinds).
3. Arm a track for recording, enable monitoring if you want to hear input while armed, and record.
4. Your recording lands as a clip on the Tracker timeline. Trim, split, move, duplicate, or rename it as needed. Undo/redo covers all of this.
5. Save your project regularly (Project menu → Save). You'll get a warning if you try to close or replace unsaved work.

**What to evaluate:** does recording latency feel right, does arranging clips feel responsive, does undo/redo ever surprise you, does save/reload ever lose anything.

### 2. Load a VST3 plugin and use it

1. Open the Plugins panel. You'll only see plugins the app has already scanned into its catalog — there's no "browse for a file" option by design; use "Rescan" or "Manage VST folders..." if a plugin you expect isn't listed.
2. Load a plugin onto a track insert or the master insert.
3. Open its native editor window, adjust parameters, bypass it, close it.

**Known limitation:** a single insert slot per track and one master insert both work reliably. Multi-slot FX chains per track (loading several plugins in series on one track) are not yet confirmed reliable — if you try it, that's useful data for us either way.

### 3. Render your mix to a file

Project menu → render/export the full mix to WAV. This is a real, tested path.

**Not yet available:** rendering a subset (a time range, or individual track stems) or normalization on export. Full-mix export only, for now.

### 4. Set up a MIDI control surface (X-Touch / Mackie-style)

Transport controls, channel faders, master fader, pan, mute/solo/record-arm, and bank switching all work from a connected X-Touch-style controller. You can also right-click most controls in the app and use MIDI Learn to bind your own physical control.

**Known limitation:** this covers on/off and trigger-style actions well. Continuously variable controls (a physical knob smoothly driving a parameter in real time, beyond a fader) aren't wired yet. Scribble-strip displays and specific hardware profiles (e.g. BCR2000) may be flaky — tell us specifically what hardware and what broke.

### 5. Write and compile FRust code (the "Code" tab)

A plain text editor for FRust source. Type or paste code, hit Compile, and it recompiles live as you edit. You can save your source into the project's asset library and reload it later.

This is a real, working compile loop backed by FRust's actual LLVM-based compiler — useful to explore even before FRust is wired into more of the app's audio tools.

### 6. Batch-build a sample pack

A standalone tool (reached from the Plugins panel, "Build Sample Pack...") that takes a folder of scattered single-note recordings and processes them into a usable instrument sample pack, with a live progress log.

## Explore, But Don't Expect Sound Yet

These two tools are real, substantial pieces of UI that are worth exploring and giving feedback on — as *authoring experiences* — but neither one produces audio you can hear yet. This is a known, documented gap, not something broken on your machine.

### Signal Lab (the "Signal" tab)

A visual node-graph editor for sound design: wire together sources, filters, an ADSR envelope, a mixer, and five "character" macros (Hardness/Weight/Air/Grit/Size), with live scope/analyzer previews on nodes. Building a graph here genuinely compiles down to real FRust code and JIT-executes it under the hood — but the result isn't yet connected to the app's actual audio output. You can build a patch and inspect the generated code; you can't yet press play and hear it.

**What's useful feedback here:** is the graph-building experience itself intuitive — placing nodes, wiring connections, using the macros, reading the previews? That's real, gatherable signal even without sound.

### Foley (the "Foley" tab)

A separate node-graph tool for sequencing sound cues (trigger → play sample → mix/delay → branching logic), also compiling to FRust. Like Signal Lab, this has no audio runtime behind it yet at all — nothing you build here can be triggered or heard currently.

**What's useful feedback here:** same as above — is the cue-sequencing metaphor clear, does the graph layout make sense, is anything confusing purely as a design surface.

You may notice Signal Lab and Foley's node-graph editors don't look or behave quite the same as each other — that's a known architectural inconsistency on our end (two different node-graph implementations under the hood), not something to debug on your side, but feel free to mention if the difference is jarring.

## Other Areas Worth Poking At (Less Documented, Less Verified)

- **Score panel** — a piano-roll/notation-style panel with per-note lyric fields, aimed at vocal-style composition. Not covered by our own internal testing checklist yet, so we genuinely don't know its current state as well as the areas above — a good spot-check target.
- **MIDI Editor** — opens a piano-roll editor for a MIDI clip from the timeline, with its own local playback preview. Looks reasonably complete; try it.
- **Mixer faders and pan** — the controls exist and are readable/settable, but whether every fader/pan move reliably affects what you actually hear on playback and render is still being verified internally. If you find a case where it doesn't, that's a high-value bug report.
- **Studio-grade audio I/O** (named input assignment, multi-track simultaneous recording, monitoring/meters) — internally flagged as still in progress. If you have an audio interface with multiple inputs, trying to name and route them is a good stress test.

## Not Ready for Beta Feedback Yet

Please don't spend testing time here — these are known-incomplete and tracked internally already:

- The AI assistant / "Virtual Engineer" panel — present in the UI, not functionally wired to a model provider yet.
- Anything requiring Signal Lab or Foley to produce actual sound.
- VST2 plugin support (explicitly legacy/work-in-progress; use VST3).

## How to Evaluate This Build

When giving feedback, the most useful framing is:

1. **Does the core loop hold up?** Record, arrange, mix with real plugins, export — under real use, for real material you care about, not just a quick test.
2. **Does anything documented above as "working" break for you?** That's the highest-priority class of bug.
3. **For the not-yet-wired tools (Signal Lab, Foley, AI panel), is the *authoring experience* good** even without sound/execution behind it yet? We're actively building the runtime underneath; UI/workflow feedback now is cheap to act on before that lands.
4. **Anything that feels confusing, inconsistent, or surprising** — even if it's "working as intended." A beta tester's confusion is real data regardless of whether the underlying code is "correct."

## Sending Feedback

An in-app opt-in feedback and metrics system is being built. Until it ships, use whatever channel you were given this build through to send feedback directly — screenshots, project files that reproduce an issue, and exact steps are all extremely helpful. This section will be updated once in-app feedback submission is available.
