#pragma once

#include <string>

// Signal Lab's first vertical-slice proof: node graph -> generated FRust
// source -> compiled -> JIT-executed via the same shared
// creation::frust::PluginRuntime every other app's node graphs already run
// through -> real numeric result, using Station's own node catalog
// (AudioNodeCatalog.h) and codegen call site (AudioGraphCodegen.h). This is
// Station's first step toward an Engine-`world_runtime.h`-style domain
// runtime -- see docs/Signal-Lab-Node-Graph-Spec.md's "Compilation &
// cross-suite execution model" section for the full roadmap this is the
// first slice of.
//
// Deliberately minimal: proves the pipeline on ONE computed sample via a
// real sine_oscillator FRust function (BuiltInPlugins/
// SignalLabNodesLibrary.frust) calling a host-registered `sin` extern, not
// a real N-sample buffer render yet (that needs an Array<f32, N> host
// buffer and a real accumulating phase -- next slice, not this one).

namespace cw::audionodes
{

struct SelfTestResult
{
    bool ok = false;
    std::string message;
    float computedSample = 0.0f;
    float expectedSample = 0.0f;
};

// Builds the Sine->Output demo graph, generates FRust source from it,
// compiles and runs it via the real JIT, and compares the result against
// the same computation done natively in C++ (std::sin(phase) * level) --
// the acceptance bar from the plan's vertical slice.
SelfTestResult RunAudioGraphSelfTest();

} // namespace cw::audionodes
