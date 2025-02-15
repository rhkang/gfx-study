#include "base.hlsl"

Texture2D textureSampler : register(t0, space1);
SamplerState textureSamplerState : register(s0, space1);

cbuffer UBO : register(b0)
{
	float4x4 model;
	float4x4 view;
	float4x4 projection;
};

VSOutput vert(VSInput input) {
    VSOutput output;
    output.pos = mul(projection, mul(view, mul(model, float4(input.pos, 1.0))));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 frag(
    [[vk::location(0)]] float2 uv : TEXCOORD0,
    [[vk::location(1)]] float4 color : COLOR0) : SV_TARGET
{
    float4 sampledColor = textureSampler.Sample(textureSamplerState, uv);
    return sampledColor;
}