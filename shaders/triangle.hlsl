#include "base.hlsl"

VSOutput vert(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 frag([[vk::location(1)]] float4 color : COLOR0) : SV_TARGET
{
    return color;
}