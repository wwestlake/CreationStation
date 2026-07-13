#pragma once

#include <JuceHeader.h>

namespace djehuti::router::contract
{
inline constexpr int sampleRate = 48000;
inline constexpr int framesPerBuffer = 480;
inline constexpr int bufferLatencyMs = 10;

inline constexpr const char* productName = "Djehuti Router";
inline constexpr const char* shortName = "DjeRoute";

inline constexpr const char* captureEndpoint = "Djehuti Router Capture";
inline constexpr const char* monitorEndpoint = "Djehuti Router Monitor";
inline constexpr const char* micEndpoint = "Djehuti Router Mic";
inline constexpr const char* instrumentEndpoint = "Djehuti Router Instrument";

inline juce::StringArray endpointNames()
{
    return { captureEndpoint, monitorEndpoint, micEndpoint, instrumentEndpoint };
}
}
