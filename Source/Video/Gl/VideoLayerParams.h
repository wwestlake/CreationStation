#pragma once

#include <JuceHeader.h>

#include "VideoGlView.h"

namespace cs::videoparams
{
// A video clip's own effect and layout settings live on the clip as a plain named-value set, saved with the timeline.
// Keeping them as named values (not a fixed struct) means a new effect adds keys without changing the file format,
// and a FRust node or automation lane addresses a setting by its name - the same way for every effect.
inline constexpr const char* keyEnabled = "key.enabled";
inline constexpr const char* keyRed = "key.r";
inline constexpr const char* keyGreen = "key.g";
inline constexpr const char* keyBlue = "key.b";
inline constexpr const char* keyTolerance = "key.tolerance";
inline constexpr const char* keySoftness = "key.softness";
inline constexpr const char* keySpill = "key.spill";
inline constexpr const char* layoutX = "xf.x";
inline constexpr const char* layoutY = "xf.y";
inline constexpr const char* layoutScale = "xf.scale";
inline constexpr const char* layoutOpacity = "xf.opacity";

inline float number(const juce::NamedValueSet& p, const char* key, float fallback)
{
    return (float) (double) p.getWithDefault(key, fallback);
}

// The renderer's layer for one clip: its picture plus its settings.
inline VideoLayer toLayer(juce::Image frame, const juce::NamedValueSet& p)
{
    VideoLayer layer;
    layer.frame = std::move(frame);
    layer.keyEnabled = (bool) p.getWithDefault(keyEnabled, false);
    layer.keyColor[0] = number(p, keyRed, 0.0f);
    layer.keyColor[1] = number(p, keyGreen, 1.0f);
    layer.keyColor[2] = number(p, keyBlue, 0.0f);
    layer.tolerance = number(p, keyTolerance, 0.30f);
    layer.softness = number(p, keySoftness, 0.10f);
    layer.spill = number(p, keySpill, 0.50f);
    layer.x = number(p, layoutX, 0.0f);
    layer.y = number(p, layoutY, 0.0f);
    layer.scale = number(p, layoutScale, 1.0f);
    layer.opacity = number(p, layoutOpacity, 1.0f);
    return layer;
}
}
