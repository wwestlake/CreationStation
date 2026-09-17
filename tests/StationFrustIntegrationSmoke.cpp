#include <cmath>
#include <iostream>
#include <string>

#include <creation/frust/PluginRuntime.h>

#include "Language/AudioGraphSelfTest.h"

namespace
{
using SineFn = double (*)(double, double);
}

int main()
{
    const auto graphResult = cw::audionodes::RunAudioGraphSelfTest();
    if (!graphResult.ok)
    {
        std::cerr << graphResult.message << '\n';
        return 1;
    }

    creation::frust::PluginRuntime runtime("creation-station-signal-lab");

    std::string error;
    if (!runtime.load(CS_SIGNAL_LAB_RUNTIME, error))
    {
        std::cerr << "Could not load the production Signal Lab FRust node library: " << error << '\n';
        return 2;
    }

    const auto sine = reinterpret_cast<SineFn>(runtime.getFunction("render_sine"));
    if (sine == nullptr)
    {
        std::cerr << "The production Signal Lab FRust runtime did not export render_sine.\n";
        return 3;
    }

    constexpr double phase = 0.25;
    constexpr double level = 0.4;
    const double actual = sine(phase, level);
    const double expected = std::sin(phase) * level;
    if (std::abs(actual - expected) >= 0.0000001)
    {
        std::cerr << "Production FRust sine mismatch: expected " << expected << ", got " << actual << '\n';
        return 4;
    }

    std::cout << "Station FRust integration smoke passed: graph JIT=" << graphResult.computedSample
              << ", production sine=" << actual << '\n';
    return 0;
}
