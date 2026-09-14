# Signal Lab Node Graph Spec

Living requirements document for Signal Lab's node-graph system. Update this
file every time a requirement is given or changed — do not let requirements
live only in chat. When a requirement changes, edit the relevant section in
place and note it in the Change Log at the bottom; don't leave the old and
new versions both standing.

Status tags used below: **[DONE]** implemented and built, **[IN PROGRESS]**
being implemented now, **[SPECIFIED]** requirement is locked in but not yet
built, **[OPEN]** requirement is real but the design isn't settled yet.

## Node anatomy

Every node has two independent kinds of port, both visible on the node box:

- **Signal ports** — white square icon with a triangle, top-left (in) /
  top-right (out) of the node. This carries the actual continuous audio
  signal through the chain — it is a data pin, not a Blueprint-style
  execution/control-flow trigger (see Runtime model below: this is a
  Material-graph style graph, not a Blueprint-style one). Only present where
  a node actually has signal in/out — sources have signal-out only, Output
  has signal-in only. Positioned slightly inset from the exact corner, not
  flush on the edge. **[DONE]**
- **Typed data ports** — colored circles. Inputs stack down the left edge,
  outputs stack down the right edge. Color is by value type and applies to
  **both** inputs and outputs (not just inputs) — float = red, int = blue,
  bool = green. **[DONE]**
- **Type semantics**: Float = continuous modulation (level, cutoff,
  pitch...). Int = categorical/enum selection (e.g. a `surface` parameter:
  gravel/grass/concrete — picks a branch, not a blend). Bool = on/off
  toggle (enable/disable a stage). This matters for codegen: a Bool input
  likely gates whether a code path is even emitted/called, not just a
  value passed through.
- **Connection lines** are colored to match the port type they connect
  (white for exec, red/blue/green for data). **[DONE]**
- Every non-exec port gets a small label next to its dot (e.g. "Cutoff",
  "Level", "Signal") so it's identifiable without crowding the node body.
  Compact always-on label for now; a hover tooltip with fuller detail may be
  added later if the compact label isn't enough. **[DONE]**

## Node sizing

- Node height is **not fixed** — it grows to fit however many ports/content
  rows a node actually needs. **[DONE]**
- Some node types show additional content in the node's central body area —
  e.g. the Scope node renders a small live mini-trace inline, the Analyzer
  renders mini spectrum bars inline. Double-clicking / opening the node
  still gives the full detachable instrument window ("the full deal") —
  the inline view is a compact preview, not a replacement. **[DONE]**
  (mini-preview rendering pre-existed; height now reserves room for it.)

## Parameter ports (inputs) — NOW requirement, not deferred

- A node must expose **every parameter the user can edit in its inspector
  panel** as its own colored input port on the node in the graph — one port
  per slider/field, not one generic combined port. E.g. Filter exposes
  Cutoff, Resonance, Env Amount as three separate ports; Output exposes
  Pitch + the five character macros as six separate ports. **[IN PROGRESS]**
  — port list is driven by the same parameter set already used by the
  existing automation/timeline modulation system
  (`getAutomationTargetSpecs()` in `SignalLabPanel.cpp`), mapped per node
  type via `nodeParameterIds()`.

## Parameter ports (outputs) — real requirement, design not settled

- Outputs are **not** a mirror of inputs. Most nodes will not output any
  parameter at all (only their main signal, if they have one). Some nodes
  do output parameters, and what they output is specific to that node —
  e.g. an **automation/timeline node's output is the parameter value it is
  varying over time**, not a fixed generic value. **[OPEN]** — this needs
  its own design pass (which node types get parameter outputs, and what
  exactly they output) before implementation. Documented here so it is not
  lost or treated as "later" in a way that drops it — it is an accepted
  requirement, just not yet designed/built.

## Connections

- Right-click empty canvas → searchable node list (type to filter) → click
  an entry → node is placed at the mouse position that was right-clicked.
  **[DONE]**
- Drag from an output port to a compatible input port (same color/type,
  correct direction) creates a connection. **[DONE]**
- **UE4-style reroute points**: right-click a wire → "Add Reroute Point
  Here" inserts a draggable bend point; drag it to reshape the wire's path;
  right-click a reroute point → "Remove Reroute Point". **[DONE]**
- Click a wire to select it; Delete/Backspace removes it. Right-click a wire
  also offers "Delete Connection" directly. **[DONE]**
- This is UI/data-model only so far — connections are stored and drawn but
  do **not yet drive actual audio routing**. That is the next phase after
  the UI is solid (see Runtime model below). **[SPECIFIED, not built]**

## Runtime execution model — how a connected graph actually runs

This is a **UE4 Material-graph style graph, not a Blueprint-style graph.**
There is no discrete step-by-step execution chain (Blueprint's white exec
pins mean "do this, then do that"). Signal Lab's white "Signal" ports carry
the actual continuous audio signal being processed through the chain
(source → mix → filter → envelope → output) — they are a data pin, not a
control-flow trigger, same as how a Material graph has no execution pins at
all and just continuously evaluates dataflow. **[DONE]** — `getNodePorts()`
already models it this way: a "Signal" port only appears where a node has
an actual signal in/out (sources: signal-out only; Output: signal-in only),
colored/shaped white to visually set it apart from the typed scalar
parameter ports, but it is not a separate abstract "run" trigger.

From the sequence-diagram sketch (`[PARAM SOURCE]` / `[NODE]` lanes):

- A global **"Audio Stream Start"** event kicks off the global audio stream.
- The stream reaches each **[NODE]**.
- When a node needs a parameter value, it sends a **"Parameter Request"**
  back to whatever Param Source node feeds that input.
- The Param Source computes the value on demand via an **"Internal
  Parameter Generation Loop"** (its own internal logic — e.g. an LFO, a
  curve, a script) and returns it.
- This is a **pull-based** model — a node asks for a parameter value when it
  needs it, not a continuous per-sample push down a wire.

**[OPEN]** — not yet implemented. This is the "connect the data model
underneath" phase; touches `SignalGraphRuntime`, not just the UI.

## Sources vs Sinks

- Signal Lab's node categories should be **Sources** and **Sinks** (not just
  a single implicit "Output"). **[OPEN / SPECIFIED, not built]**
- First concrete Sink to add: **Device Sink** — lets the user pick where
  audio actually goes:
  - **App Default** — this app's own configured output device (not
    necessarily the OS default).
  - **System Default** — whatever the OS currently considers default.
  - **A specific named device** — from the real enumerated output device
    list (user has numerous output devices).
  - Station already has real audio-device infrastructure elsewhere
    (`WorkstationAudioEngine` — device enumeration, ASIO4ALL, etc.) that a
    Device Sink node should reuse rather than re-implementing.
- The existing single "Output" node (Sound Name / Duration / Pitch /
  character macros) still needs a design decision: does it stay as-is and
  Device Sink is simply a new, separate, addable sink alongside it, or does
  "Output" get absorbed into the Sinks category as the graph's terminal
  node? **[OPEN — needs a decision before Sinks work starts]**

## Grouping / user-defined nodes — concept, not yet designed in detail

- Multiple nodes can be selected and **grouped/collapsed into a single node**
  that behaves like any other node from the outside (a reusable
  function/subgraph). **[OPEN]**
- Inside the group, there is a **special "Group Output" node** — whatever
  gets wired into it becomes the collapsed group node's external Signal-out
  port. (Mirrors UE4 Material Functions' Output node / Blueprint Function's
  return node.)
- These packages are **generators, not processors/effects** — a packaged
  FRust sound generator has **no external Signal-in port at all**, only
  Signal-out. It produces sound; it does not take sound in. It can still
  have external **parameter-in ports** (pitch, macros, whatever the group
  exposes for control), just never an audio signal input at the package
  boundary. **[SPECIFIED]**
- Open question to settle before building: is a grouped node a **true
  reusable/shareable asset** (a real packaged FRust function you can reuse
  across different sound designs — this matches the Epic #30 north star:
  "supports reusable packaged graphs/functions backed by FRust"), or just a
  **local collapse-to-tidy-the-graph** convenience scoped to one sound
  design? This changes the implementation a lot (save/load/reference a
  named asset vs. a purely in-memory subgraph).
- Likely also needs: which of the group's internal parameter ports get
  promoted to the group boundary (not just the one Signal-out) — not yet
  designed.

## Grouped nodes are named, reusable, FRust-backed assets (answers the open question above)

- Confirmed: a grouped node is a **real, named, reusable asset** — not just a
  local collapse-to-tidy-the-graph convenience. **[SPECIFIED]**
- Nodes represent FRust functions or internal functions. The built-in/primitive
  nodes ("internal functions") are implemented natively in C++. User-composed
  nodes/functions can additionally be authored in **FRust**, or in a **"Node
  lang"** (a textual representation of a node graph — exact relationship to
  FRust not yet settled). **[SPECIFIED, mechanism OPEN]**

## Serialization / model round-trip

- From a node diagram, the system must produce a **JSON description**
  covering: node layout, connections, and parameter settings. This JSON
  **is the model** — it must be loadable back into the editor (full
  round-trip). **[SPECIFIED, not yet built]** — this supersedes/extends the
  existing flat `PatchDocument` JSON format (`PatchModel.h`), which doesn't
  currently capture node positions or typed-port connections.

## VERTICAL SLICE PROVEN WORKING (2026-08-08; re-proven on FRust 2026-09-13)

The full pipeline — node graph → generated FRust text → parsed → sema/
codegen-checked → JIT-compiled (real LLVM, via `frust_plugin_host`) →
executed → real numeric result — has been built and verified end to end.
**[DONE]**

Originally proven on CEL (2026-08-08); the suite has since replaced CEL
outright with FRust, and this same vertical slice was re-verified on the
FRust pipeline (2026-09-13) with the same demo computation and the same
result.

What was built (all in `apps/CreationStation/Source/Language/`):
- `AudioNodeCatalog.h/.cpp` — registers `SineOscillator` (input: `level`/
  Float, output: `signalOut`/AudioSignal) and `Output` (input: `signalIn`/
  AudioSignal) into a `ce::node_system::NodeLibraryRegistry` tagged
  `Domain::Core` (see `frust_codegen.cpp`'s own comment on why the pure-
  value compile path requires `Domain::Core`, not `Domain::Audio`, despite
  the node's audio-DSP subject matter), bound to a real FRust function in
  `BuiltInPlugins/SignalLabNodesLibrary.frust`.
- `AudioGraphCodegen.h/.cpp` — Signal Lab's own graph → FRust text generator
  (deliberately separate from the control-graph compiler, per this doc's
  own architecture section above), calling the shared
  `ce::node_system::CompileBehaviorGraphToFrust` (`shared/NodeSystem/src/
  frust_codegen.cpp`) every exec-chain/dataflow graph in the suite compiles
  through. Walks back from the Output node's connection to find the
  feeding SineOscillator, reads its `level` pin's literal value, and emits
  a real `.frust` function.
- `AudioGraphSelfTest.h/.cpp` — builds the demo graph, generates source,
  loads it through `creation::frust::PluginRuntime` (`shared/
  FrustPluginRuntime`) after registering the `sin` host function FRust has
  no built-in math intrinsic for, and compares the JIT-computed result
  against the same computation done natively in C++.
- Result: JIT produced `0.311627` for `sin(0.5) * 0.65`, byte-matching the
  native `std::sin` computation. Verified by actually running the built
  app, not assumed.
- `CMakeLists.txt`: `creation_suite_frust_plugin_runtime` (wrapping
  `third_party/FrustLang`) is linked into `CreativeWorkstation` in place of
  the former `ce_lang_frontend`/`ce_lang_jit`/`ce_lang_nodegen` CEL
  libraries, which no longer exist anywhere in this repository.

**Deliberate v1 scope limits** (so they don't read as oversights later,
same convention this suite's other node-catalog docs use):
- Proves ONE computed sample via `sin`, registered as a host function
  because FRust has no built-in math intrinsics — **not** a real N-sample
  buffer render yet.
- The `level` parameter is read as a **literal only** — a level fed by
  another node's output (a Value node, an envelope...) isn't walked yet;
  codegen only follows the Output node's own incoming connection back one
  hop to its literal-parameter source.
- No `AudioScriptContext`, no Station-side domain runtime layer, no
  buffer-writing intrinsic yet — those are the concrete next steps (see
  "Compilation & cross-suite execution model" above), now with a proven
  foundation under them instead of an untested plan.

## MAJOR FINDING (2026-08-08): the node-graph-to-FRust system already exists

Before reading the "confirmed pipeline" section below, read this first —
it changes the recommended path significantly. Verified by reading the
actual code, not assumed. (Originally written against CEL's node system;
updated 2026-09-13 to name the current FRust-backed equivalents after the
suite-wide CEL removal — the underlying `ce::node_system` structures this
section describes did not change shape, only the language they compile to.)

`shared/NodeSystem` (`ce::node_system`, headers in
`shared/NodeSystem/include/node_system/`) is a **complete, working,
domain-agnostic node-graph system**, already built:

- `node.h` — `Domain` enum is `{ Core, Animation, Material, Event, Audio,
  Video, Input, Physics }`. Only `Core` and `Event` have real codegen
  support in `frust_codegen.cpp` today — the others exist in the enum but
  aren't wired through the compiler yet (a real, current gap, not
  something to assume works).
- `pin.h` — `PinKind { Data, Exec }`, `DataType { Float, Vec2, Vec3, Vec4,
  Color, Bool, Int, String, Transform, BoneTransform, Texture,
  **AudioSignal**, Entity }`. **`AudioSignal` is already a first-class data
  type.** This resolves the earlier "is the white Signal port an exec pin
  or a data pin" question precisely: it should be `PinKind::Data` with
  `DataType::AudioSignal`, not an `Exec` pin — Exec is reserved for actual
  control-flow/event graphs (a different authoring domain that coexists in
  the same graph system, not what Signal Lab needs). `IsConnectionCompatible()`
  already implements exactly the type-matching rule (same kind, same
  DataType for Data pins) that today's `SignalLabPanel.cpp` reimplemented
  ad hoc in `tryCompleteConnection`.
- `type_registry.h` — `NodeTypeRegistry`/`NodeLibraryRegistry` +
  `NodeTypeDescriptor` is exactly the mechanism to register a node kind
  ("Sine Oscillator", "Filter", "Device Sink", ...) with a fixed, validated
  pin signature — `AddRegisteredNode` constructs a conforming instance,
  `ValidateAgainstRegistry` catches shape drift. This is the real version
  of what `nodeParameterIds()`/`getNodePorts()` in `SignalLabPanel.cpp`
  reinvented today, informally and locally.
- `frgraph_serialization.h` — a `.frgraph` file format that round-trips a
  graph, **including each node's editor canvas position** (`Node::EditorX/
  Y`, persisted through save/load). This is very likely the JSON model
  round-trip requirement from earlier in this doc — needs confirming it's
  JSON under the hood, but the round-trip mechanism itself already exists.
- `shared/NodeSystem/src/frust_codegen.cpp` (`ce::node_system`) —
  **`CompileBehaviorGraphToFrust(graph, libraries, options)`** validates a
  graph and generates real, human-readable `.frust` source text from it
  (deliberately text, not direct AST construction — the stated reason:
  graph-authored and hand-authored FRust must provably be the same
  language sharing one parse/sema/IR-gen/JIT pipeline, not two code paths
  that could drift). Diagnostics are returned as a plain error string
  today (`FrustGraphCompileResult::error`), not yet mapped back to a
  source-line map / originating node ID the way this doc's earlier "node 7
  is broken" UX wants — that mapping doesn't exist yet.
- Also present: `core.event.tick`/`core.event.beginplay`/`core.event.
  endplay` lifecycle entry nodes (`core_control_flow.cpp`) and real
  structured control flow (`core.branch`, `core.sequence`, `core.
  randomSelect`, `core.for`, `core.while`, ...) with working `frust_
  codegen.cpp` lowering for every one. No `SubgraphEntry`/`CallSubgraph`-
  style "named reusable grouped node" mechanism exists yet — that
  "grouped node" concept from the Grouping section above is still
  genuinely unbuilt, not just undocumented.

**What this means for the recommended path:** Signal Lab should very
likely **register its own node types into `NodeLibraryRegistry`** and
**operate on `ce::node_system::Graph`/`Node`/`Pin` directly**, rather than
continuing to grow `SignalLabPanel.cpp`'s own bespoke `GraphNodeModel`/
`GraphConnection`/`getNodePorts()` structures, which today duplicate —
informally, and not validated the same way — what this shared system
already does properly. This is a real architectural decision to make
explicitly (migrate now vs. keep the UI-only bespoke model for a while
longer and migrate later), not something to decide implicitly by
continuing to add to the bespoke version. (Update 2026-09-13: `Audio*Node
Catalog.h/.cpp` now does exactly this — registered as `Domain::Core`, not
`Domain::Audio`, per the codegen-support note above.)

**Still genuinely missing** (confirmed by search, not assumed): the actual
DSP math a registered "Sine Oscillator" node would call into is still just
one host-registered `sin` function, not a real oscillator/filter/envelope
library, and Station has no runtime layer analogous to Engine's `world_
runtime.h`-style host wiring to plug a compiled module into Station's
actual audio render path.

## Compilation & cross-suite execution model — OPEN, needs a real design pass

This is the big open architecture question, and it reaches beyond Signal
Lab's UI into `third_party/FrustLang` and other suite apps, so a fuller
design doc may be warranted once this is scoped — captured here first so
it isn't lost.

**Confirmed pipeline, verified against the actual codebase (2026-08-08;
re-verified against the FRust-based implementation 2026-09-13):**

1. Node structure (Signal Lab's visual graph — already built).
2. Node structure compiles to **FRust source text** via the shared
   `CompileBehaviorGraphToFrust` (the graph *is* the program; hand-authored
   FRust text is only an intermediate artifact for inspection/debugging,
   not a separate authoring path).
3. Source is **JIT-compiled and executed** via `creation::frust::
   PluginRuntime` (`shared/FrustPluginRuntime`), wrapping FrustLang's own
   `frust_plugin_host` (real LLVM ORC JIT compilation to native code, not a
   spec-only concept).
4. Execution goes through **a Station-side domain runtime** — Station
   still needs to build its own audio-render host wiring (register
   Station's own audio intrinsics as host functions, following the same
   `registerHostFunction`/manifest-driven convention every other FRust
   plugin host in the suite already uses, e.g. Creation Engine's
   `EngineFrustHost`).

**What already exists vs. what Station still needs to build (verified by
reading the actual code, not assumed):**
- Shared JIT engine (compile FRust → LLVM IR → native, run it): **built**,
  self-tested, already used across the suite (Signal Lab, Foley, Creation
  Engine's Pods).
- An Engine-style domain runtime for Station (register audio intrinsics,
  host-wire a compiled module into Station's audio render path): **does
  not exist yet.** `SignalGraphRuntime` today is pure native C++ with zero
  FRust/JIT involvement.
- Node-graph → FRust compiler (the thing that turns Signal Lab's node
  model into a FRust program): **built** — `CompileBehaviorGraphToFrust`,
  called from `AudioGraphCodegen.cpp`.

Older, less precise framing this section replaces: "the node graph should
compile to CEL, and CEL then runs it — including the automations, not a
separate hardcoded modulation system." Still true with FRust standing in
for CEL, just now stated as the concrete 4-step pipeline above.
- The resulting compiled FRust function should be a **portable, suite-wide
  reusable sound generator**. General principle: **any suite member that
  uses sound can use them** — live-streamed as sound effects, and because
  it's FRust, callers can pass **parameters in at call time** (whichever
  parameter-in ports the package exposes become the FRust function's
  callable parameter signature). Named examples so far:
  - Station's own **Tracker timeline** (as a clip/event-triggered generator).
  - Station's **Foley** tool (as a sound source/generator).
  - **Creation Movie** (the video editor) — live-streamed sound effects.
  - Not an exhaustive list — the principle is suite-wide, not Station-only.
- Two consumption modes for the same compiled FRust function:
  1. **Executed live/on-demand** wherever it's needed (e.g. a Tracker
     timeline event calls it in real time).
  2. **Rendered once to WAV** by the editor and reused as a static baked
     asset (ties back to the real-time-vs-offline-render discussion above —
     baking is just one more way of consuming the same compiled function).
- Not yet decided: compilation pipeline details, how FRust's automation/curve
  execution model maps onto the pull-based "Parameter Request" runtime model
  described above, and how a Tracker/Foley call site discovers and invokes a
  named Signal Lab FRust asset.

## Rendering model — real-time vs. offline-to-WAV

- Real-time playback/preview is a **convenience, not a requirement**. Some
  graphs will be too complex/expensive to run live and must instead be
  **rendered offline to WAV** (bounced), and that render becomes the actual
  playable asset. **[OPEN]** — aligns with the already-existing "Render to
  Project" menu action and Issue #25's "editable sound-design objects saved
  as project assets, render WAV only on command" direction; needs a design
  pass on how/when a graph is flagged real-time-capable vs. render-only.

## Duration — sounds are short and finite, but bounded not fixed

- Signal Lab produces **short, finite sounds** — never infinite/indefinite.
  They can be long, but must always be **bounded**. **[OPEN — clarifying
  existing `recipe.durationSeconds` as a hard requirement, not just a UI
  field]**
- The practical mechanism that bounds duration is the **shaping/envelope
  (ADSR) node** — described as "basically a volume control automation that
  happens very quickly as a modulation on the signal." I.e. the envelope is
  a fast **amplitude/gain modulation curve over time**, not a separate
  mechanism from the parameter-modulation system already being built. This
  is a concrete example of the "some nodes DO output a parameter" case from
  the Parameter ports (outputs) section above — the Envelope node's output
  is the time-varying gain value.
- The envelope/ADSR editor's **time axis must be expandable/zoomable** — not
  a fixed tiny scale — so both short percussive envelopes and long swelling
  ones are editable comfortably. `EnvelopeEditor` already exists in
  `SignalLabPanel.h`/`.cpp`; needs a zoomable time axis. **[OPEN]**

## Mixer node — weighted sum (built)

Confirmed and built: Mixer output is a **weighted sum**:
`out = sum(input_i * weight_i)`. Each input is a pair of ports — a white
Signal (Channel N) port for the stream, and a real-valued Weight N Float
port (automatable) for its multiplier. Default 2 inputs; grown via the
node's own "+ Input" button drawn on the node body. **[DONE]**

## LFO node + generic Automation/Timeline node — needed, not built yet

From a worked example (sources → Shaper → weighted Mix, with an LFO
feeding "Mix Modulation", → Final Shaper → output):

- **LFO node**: a control-rate modulation source, distinct from the
  audio-rate oscillators (Sine/Saw/etc. produce audible signal; an LFO
  produces a slow-moving value used to modulate a parameter — e.g. it
  would feed a Mixer's Weight port to slowly crossfade between two
  sources). **[SPECIFIED, not built]**
- **Generic Automation/Timeline node**: modeled on UE4's Timeline node —
  an editable curve over time, evaluated at the current playback
  position, output wired into whatever parameter it drives. Key
  difference from UE4: **time here is samples, not ticks/frames** —
  sample-accurate, matching the audio-rate domain this graph runs in.
  **[SPECIFIED, not built]**
- Also needed for the worked example: a **Shaper (waveshaper/distortion
  curve)** node type — signal in, nonlinear transfer curve, signal out.
  Not yet in the node catalog. **[SPECIFIED, not built]**

## Graph-level Duration — first-class metadata, not buried in Output

Every graph has a specified length, established up front as real graph
metadata — not just a field buried in the Output node's inspector.
**Default 5 seconds** (was 1.5s). Build the sound, then adjust the
timeline length up or down as needed — same "short, finite, bounded"
principle as before, just surfaced properly instead of hidden.
**[IN PROGRESS]**

## Future ideas — noted, not being built yet

- **N-channel Scope with real capture behavior**: today's Scope node is a
  fixed 2-channel (A/B) mini preview with just gain sliders — no real
  trigger logic, no per-channel add/remove. Wanted: genuine oscilloscope-
  like signal capture (proper triggering, multiple simultaneous traces),
  channel count expandable to at least 4 (with a channel-add control) —
  functional parity with a real scope, not visual styling. **[OPEN,
  future]**
- **MIDI control-surface binding for Signal Lab parameters**: tracked as
  [CreationStation#40](https://github.com/wwestlake/CreationStation/issues/40).
  The "right-click -> Learn MIDI Binding" UI/storage
  (`MainComponent::showMidiLearnDialog`, `WorkstationAudioEngine::
  armMidiLearn`, `ControlSurfaceMappingStore`) is real and reusable, but
  the binding store today only supports discrete/momentary actions (e.g.
  transport play/stop) — there is no dispatch path yet for an incoming
  continuous CC value to drive a bound parameter's live value. That
  dispatch path is the real remaining work. **[OPEN, future — issue
  filed, not being built now]**

## Already-shipped, adjacent Signal Lab work (for context, not node-graph itself)

- Signal Lab always opens **blank** — no auto-restore from project/session
  state. Only the explicit Signal menu (New / Open / Save / Save As /
  Render to Project) loads or persists anything. **[DONE]**
- Top Properties box: editable **Sound Name** + **Description**, always
  visible even on a blank graph, saved via the Signal menu. **[DONE]**
- Variables panel: scrollable list, Add/Remove per row, Selected Variable
  panel with Name/Description/Type/Value/**Automated** checkbox — checking
  Automated grays out (disables) the Value field. **[DONE]**

## Change Log

- 2026-08-08: Initial doc created, capturing everything specified across the
  session so far (node anatomy, sizing, parameter input ports as a NOW
  requirement, parameter outputs as an OPEN requirement, connections/reroute,
  runtime pull model, Sources/Sinks + Device Sink).
- 2026-09-13: CEL removed suite-wide, replaced by FRust everywhere. Updated
  every CEL-era reference in this doc (API names, file paths, `.celg` ->
  `.frgraph`, `ce_lang_*` libraries -> `creation_suite_frust_plugin_runtime`)
  to the current FRust-based implementation; re-verified the "VERTICAL SLICE
  PROVEN WORKING" pipeline against the FRust rewrite with the same result.
  Also added Foley's own FRust node catalog and a new shared `core.
  randomSelect` control-flow primitive (`ControlFlowKind::RandomSelect`,
  `shared/NodeSystem`) while doing this pass.
