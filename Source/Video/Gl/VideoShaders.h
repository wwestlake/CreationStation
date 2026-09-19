#pragma once

#include <JuceHeader.h>

namespace cs::videogl
{
// The GLSL for the video renderer, embedded so it ships inside the app. ShaderComposer resolves "programs/..." and
// "library/..." paths against this table exactly as it does against files on disk (#include and #define work the
// same way). Generated shader text (the GLSL a node graph compiles to) can be served through the same provider.
inline bool findVideoShaderSource(const juce::String& path, juce::String& out)
{
    static const char* kFullscreenVert = R"glsl(#version 330 core
// One triangle that covers the whole target: no vertex buffer, the corners come from the vertex id.
out vec2 vUV;

void main()
{
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

    static const char* kCommon = R"glsl(
// ---- shared video helpers -------------------------------------------------------------------------------------

// A dark two-tone checker, drawn behind the picture so transparency (a keyed-out background) is visible.
vec3 transparencyChecker(vec2 pixel)
{
    vec2 cell = floor(pixel / 12.0);
    float tone = mod(cell.x + cell.y, 2.0);
    return mix(vec3(0.10, 0.11, 0.13), vec3(0.16, 0.17, 0.20), tone);
}

// BT.601 chroma: the colour part of a pixel with brightness taken out, so a key works on hue even where the
// screen is lit unevenly.
vec2 chromaOf(vec3 rgb)
{
    float cb = -0.168736 * rgb.r - 0.331264 * rgb.g + 0.5 * rgb.b;
    float cr = 0.5 * rgb.r - 0.418688 * rgb.g - 0.081312 * rgb.b;
    return vec2(cb, cr);
}

// 0 where the pixel is the key colour, 1 where it is anything else, with a soft edge of `softness` beyond `tolerance`.
float chromaKeyAlpha(vec3 rgb, vec3 keyColor, float tolerance, float softness)
{
    float distanceToKey = length(chromaOf(rgb) - chromaOf(keyColor));
    return smoothstep(tolerance, tolerance + max(softness, 0.0001), distanceToKey);
}

// Pulls key-coloured light off the subject's edges: the strongest key channel is limited to the average of the others.
vec3 suppressSpill(vec3 rgb, vec3 keyColor, float amount)
{
    float keyStrength = max(max(keyColor.r, keyColor.g), keyColor.b);
    if (keyStrength <= 0.0)
        return rgb;

    vec3 weight = keyColor / keyStrength;
    float others = (rgb.r * (1.0 - weight.r) + rgb.g * (1.0 - weight.g) + rgb.b * (1.0 - weight.b))
                   / max(3.0 - (weight.r + weight.g + weight.b), 0.0001);
    float keyChannel = dot(rgb, weight) / max(weight.r + weight.g + weight.b, 0.0001);
    float excess = max(keyChannel - others, 0.0);
    return rgb - weight * excess * clamp(amount, 0.0, 1.0);
}
)glsl";

    static const char* kFrameFrag = R"glsl(#version 330 core
#include "library/video_common.glsl"

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uFrame;
uniform int uKeyEnabled;
uniform vec3 uKeyColor;
uniform float uTolerance;
uniform float uSoftness;
uniform float uSpill;

void main()
{
    vec3 rgb = texture(uFrame, vUV).rgb;
    float alpha = 1.0;

    if (uKeyEnabled != 0)
    {
        alpha = chromaKeyAlpha(rgb, uKeyColor, uTolerance, uSoftness);
        rgb = suppressSpill(rgb, uKeyColor, uSpill);
    }

    fragColor = vec4(mix(transparencyChecker(gl_FragCoord.xy), rgb, alpha), 1.0);
}
)glsl";

    if (path == "programs/video_fullscreen.vert") { out = kFullscreenVert; return true; }
    if (path == "programs/video_frame.frag") { out = kFrameFrag; return true; }
    if (path == "library/video_common.glsl") { out = kCommon; return true; }
    return false;
}
}
