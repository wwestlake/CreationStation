#pragma once

#include "node_system/frust_codegen.h"
#include "node_system/graph.h"
#include "node_system/node_library.h"

// Signal Lab's own graph -> FRust call-site glue: not a codegen engine in
// its own right (that's the shared, suite-wide node_system::
// CompileBehaviorGraphToFrust every app's node graphs already compile
// through -- see frust_codegen.h). This file's actual job is narrower: walk
// this specific v1 graph shape (one SineOscillator feeding one Output) to
// find the real source node and its literal inputs, then hand
// CompileBehaviorGraphToFrust exactly the FrustGraphCompileOptions it needs.
//
// v1 scope: proves the graph -> FRust -> JIT -> real numeric result
// pipeline on the simplest possible case (BuiltInPlugins/
// SignalLabNodesLibrary.frust's sine_oscillator, a plain scalar f64
// function -- no struct/array construction in the hot path, see
// LANGUAGE_GAPS.md's Array<N> entry for why that matters). A real
// buffer-filling render (N samples, host-provided Array<f32, N> buffer)
// needs its own follow-on work, not attempted here. See
// docs/Signal-Lab-Node-Graph-Spec.md.

namespace cw::audionodes
{

// Builds the trivial two-node demo graph the vertical slice proves the
// pipeline on: one SineOscillator (level = levelValue) connected to one
// Output. Node ids/positions are arbitrary -- this graph is never saved,
// only compiled.
ce::node_system::Graph BuildSineToOutputDemoGraph(const ce::node_system::NodeLibraryRegistry& libraries,
                                                   float levelValue);

// Validates `graph` against `libraries` and, if that passes, walks back
// from the Output node's connected signal source, then compiles a single
// FRust function (named `functionName`) computing one sample by calling
// that source node's real FRust entry point. `level` comes from the
// SineOscillator node's `level` input pin's literal value (v1:
// unconnected/literal only -- a level fed by another node's output is a
// later slice, once parameter ports actually drive codegen instead of just
// existing visually).
ce::node_system::FrustGraphCompileResult GenerateAudioSource(const ce::node_system::Graph& graph,
                                                               const ce::node_system::NodeLibraryRegistry& libraries,
                                                               const std::string& functionName);

} // namespace cw::audionodes
