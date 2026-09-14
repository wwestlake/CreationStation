#pragma once

#include "node_system/node_library.h"

// Foley's own node catalog -- deliberately separate from Signal Lab's
// (Source/Language/AudioNodeCatalog.h), per the suite's explicit
// consolidation stance: share the generic node-graph editing machinery
// (ce::node_system::Graph/NodeLibraryRegistry, and the ported
// NodeGraphComponent/NodeInspector/NodePalette UI in shared/NodeEditorUI)
// across every domain, but never merge domain-specific node catalogs.
//
// Foley is picture-sync sound design: placing/performing sounds in sync
// with a video, sequenced with choice logic (branch, random pick, delay).
// An entry point is OnTrigger (an event/cue firing) rather than Signal
// Lab's plain dataflow shape -- this is an exec-chain Behavior graph,
// compiled through the exact same shared node_system::CompileBehaviorGraphToFrust
// every other exec-chain graph in the suite already uses (see
// CreationEngine's PodEditorPanel for the reference "one function per Event
// node" pattern this mirrors).
//
// Branch/Sequence/Random Select are NOT redefined here -- they're the
// shared, generic core.branch/core.sequence/core.randomSelect control-flow
// primitives (shared/NodeSystem/core_control_flow.h), reused as-is. Only
// OnTrigger/PlaySample/GainMix/Delay are genuinely Foley-specific.

namespace cw::foleynodes
{

namespace NodeType
{
inline constexpr const char* OnTrigger = "foley.onTrigger";
inline constexpr const char* PlaySample = "foley.playSample";
inline constexpr const char* GainMix = "foley.gainMix";
inline constexpr const char* Delay = "foley.delay";
} // namespace NodeType

namespace PinName
{
inline constexpr const char* Execute = "execute";
inline constexpr const char* Then = "then";
inline constexpr const char* SampleName = "sampleName";
inline constexpr const char* Gain = "gain";
inline constexpr const char* Seconds = "seconds";
} // namespace PinName

// Real native host-extern function names PlaySample/GainMix/Delay compile
// down to (`extern fn ...;`, auto-declared by frust_codegen.cpp's exec-chain
// lowering). A host wanting to actually run a compiled Foley graph must
// register all three via creation::frust::PluginRuntime::registerHostFunction
// before load() -- same convention as Signal Lab's "sin" and CreationEngine's
// "engine_*" functions. No native implementation exists yet in this app
// (FoleyPanel today only generates and displays FRust source, the same
// scope its CEL-backed predecessor had -- see FoleyPanel.h's own comment).
inline constexpr const char* kPlaySampleHostFunction = "foley_play_sample";
inline constexpr const char* kGainMixHostFunction = "foley_gain_mix";
inline constexpr const char* kDelayHostFunction = "foley_delay_seconds";

// Builds a fresh registry containing every Foley node type plus the shared
// core control-flow primitives (Branch/Sequence/Random Select/...) Foley's
// graphs are allowed to use. Returned by value, same reasoning as Signal
// Lab's BuildAudioNodeCatalog: registrations are cheap, callers (the Foley
// panel, codegen, tests) shouldn't have to share one global instance.
ce::node_system::NodeLibraryRegistry BuildFoleyNodeCatalog();

} // namespace cw::foleynodes
