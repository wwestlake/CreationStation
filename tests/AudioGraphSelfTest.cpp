#include "AudioGraphSelfTest.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include <creation/frust/PluginRuntime.h>

#include "AudioGraphCodegen.h"
#include "AudioNodeCatalog.h"

namespace cw::audionodes
{

namespace
{
constexpr const char* kFunctionName = "compute_sample";
using ComputeSampleFn = double (*)();
} // namespace

SelfTestResult RunAudioGraphSelfTest()
{
    SelfTestResult result;

    constexpr float kLevel = 0.65f;
    constexpr float kTestPhase = 0.5f; // must match SignalLabNodesLibrary.frust's own hardcoded phase

    auto libraries = BuildAudioNodeCatalog();
    auto graph = BuildSineToOutputDemoGraph(libraries, kLevel);

    const ce::node_system::FrustGraphCompileResult generated = GenerateAudioSource(graph, libraries, kFunctionName);
    if (!generated.ok)
    {
        result.message = "codegen failed: " + generated.error;
        return result;
    }

    // Same pattern Creation Engine's own generic-node-codegen smoke test
    // uses: write the generated root alongside a real copy of the .frust
    // node-library module it `use self::`-imports (CS_SIGNAL_LAB_NODE_LIBRARY,
    // a compile-time absolute path into this repo's own BuiltInPlugins/),
    // since frust_plugin_load() needs both files sitting next to each
    // other on disk to resolve the import.
    const auto generatedDirectory = std::filesystem::temp_directory_path() / "djehuti_station_signal_lab_self_test";
    std::filesystem::create_directories(generatedDirectory);
    std::filesystem::copy_file(CS_SIGNAL_LAB_NODE_LIBRARY, generatedDirectory / "SignalLabNodesLibrary.frust",
                                std::filesystem::copy_options::overwrite_existing);
    const auto generatedPath = generatedDirectory / "GeneratedSignalLabGraph.frust";
    {
        std::ofstream generatedFile(generatedPath);
        generatedFile << generated.source;
    }

    creation::frust::PluginRuntime runtime("djehuti-station");
    // sine_oscillator's `extern fn sin(x: f64) -> f64;` resolves to this --
    // FRust has no built-in math intrinsics, so every host application
    // wiring up SignalLabNodesLibrary.frust must register this itself, same
    // as any other host-extern function convention in this suite.
    runtime.registerHostFunction("sin", reinterpret_cast<void*>(static_cast<double (*)(double)>(std::sin)));

    std::string loadError;
    if (!runtime.load(generatedPath.string(), loadError))
    {
        result.message = "generated Signal Lab FRust source did not load: " + loadError;
        return result;
    }

    const auto computeSample = reinterpret_cast<ComputeSampleFn>(runtime.getFunction(kFunctionName));
    if (computeSample == nullptr)
    {
        result.message = "could not resolve '" + std::string(kFunctionName) + "' in the loaded plugin";
        return result;
    }

    result.computedSample = static_cast<float>(computeSample());
    result.expectedSample = std::sin(kTestPhase) * kLevel;

    const float difference = std::abs(result.computedSample - result.expectedSample);
    result.ok = difference < 0.0001f;
    result.message = result.ok
        ? "OK: graph -> FRust -> JIT produced " + std::to_string(result.computedSample)
              + ", matching native std::sin computation " + std::to_string(result.expectedSample)
        : "MISMATCH: JIT produced " + std::to_string(result.computedSample)
              + ", native expected " + std::to_string(result.expectedSample);
    return result;
}

} // namespace cw::audionodes
