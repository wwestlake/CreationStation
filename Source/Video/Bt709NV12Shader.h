#pragma once

namespace cs
{
// HLSL SM4.0 shader converting an NV12 frame (separate luma/chroma SRVs, since a single NV12
// texture needs two different-format views onto its two planes) to RGBA via the BT.709
// limited-range YUV->RGB matrix used by HD/4K video. Full-screen triangle generated from
// SV_VertexID - no vertex/index buffer needed.
inline constexpr char kBt709NV12ShaderSource[] = R"hlsl(
Texture2D lumaTexture : register(t0);
Texture2D chromaTexture : register(t1);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((id << 1) & 2, id & 2);
    output.uv = uv;
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float y = lumaTexture.Sample(linearSampler, input.uv).r;
    float2 chroma = chromaTexture.Sample(linearSampler, input.uv).rg;

    // Limited-range (16-235 luma, 16-240 chroma) -> full-range, then the BT.709 matrix.
    float yy = (y - 16.0 / 255.0) * (255.0 / 219.0);
    float u = (chroma.r - 128.0 / 255.0) * (255.0 / 224.0);
    float v = (chroma.g - 128.0 / 255.0) * (255.0 / 224.0);

    float r = yy + 1.5748 * v;
    float g = yy - 0.1873 * u - 0.4681 * v;
    float b = yy + 1.8556 * u;

    return float4(saturate(float3(r, g, b)), 1.0);
}
)hlsl";
}
