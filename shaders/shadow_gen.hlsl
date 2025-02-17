struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4 color : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_Position;
};

cbuffer UBO : register(b0)
{
	float4x4 view;
	float4x4 projection;
    float4x4 lightView;
    float4x4 lightProjection;
    float3 lightPos;
    float shadowBias;
    float enablePCF;
};

struct PushConstant
{
    float4x4 model;
};

[[vk::push_constant]] PushConstant pushConstants;

VSOutput vert(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(pushConstants.model, float4(input.pos, 1.0));
    float4 viewPos = mul(lightView, worldPos);
    output.pos = mul(lightProjection, viewPos);

    return output;
}