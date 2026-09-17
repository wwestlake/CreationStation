#pragma once

#include "node_system/node_library.h"

// Signal Lab's own domain-level node library -- deliberately separate from
// the shared suite-wide Core/Event control-graph catalog (CoreNodesLibrary.
// frust in Creation Engine's BuiltInPlugins). That library models control
// logic (OnStart/OnTick/Branch, exec-chain graphs) -- the "control surface"
// every app uses. Signal Lab is a different kind of graph: a domain-level
// signal-flow process (continuous dataflow, no execution steps), so it gets
// its own library and its own codegen call site (see AudioGraphCodegen.h),
// even though it reuses the same underlying node_system::Graph/Node/Pin/
// NodeLibraryRegistry data structures and the same shared
// CompileBehaviorGraphToFrust compiler (node_system/frust_codegen.h) every
// other app's node graphs already compile through (Domain::Audio and
// DataType::AudioSignal already exist there for exactly this purpose).
//
// This is v1 scope: just enough node types (SineOscillator, Output) to
// prove the graph -> FRust -> JIT -> JUCE DSP pipeline end to end on the
// simplest possible case. See docs/Signal-Lab-Node-Graph-Spec.md for the
// full roadmap this is the first slice of.

namespace cw::audionodes
{

namespace NodeType
{
inline constexpr const char* SineOscillator = "SineOscillator";
inline constexpr const char* Output = "Output";
} // namespace NodeType

namespace PinName
{
inline constexpr const char* SignalIn = "signalIn";
inline constexpr const char* SignalOut = "signalOut";
inline constexpr const char* Phase = "phase";
inline constexpr const char* Level = "level";
} // namespace PinName

// The real FRust module SineOscillator's logic actually lives in --
// BuiltInPlugins/SignalLabNodesLibrary.frust, alongside this library's
// registered NodeTypeDescriptor::frustEntryPoint naming its function.
inline constexpr const char* kSignalLabFrustModule = "SignalLabNodesLibrary";

// Builds a fresh library containing every v1 Signal Lab node type, tagged
// ce::node_system::Domain::Audio and bound to its real FRust entry point
// in BuiltInPlugins/SignalLabNodesLibrary.frust. Returned by value, same
// reasoning as Creation Engine's own BuildCoreNodeCatalog-style builders:
// registrations are cheap, callers (codegen, the UI, tests) shouldn't have
// to share one global instance.
ce::node_system::NodeLibraryRegistry BuildAudioNodeCatalog();

} // namespace cw::audionodes
