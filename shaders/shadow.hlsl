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
    [[vk::location(0)]] float3 normal : NORMAL0;
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float4 shadowCoord : TEXCOORD1;
    [[vk::location(3)]] float3 lightDir : TEXCOORD2;
};

Texture2D shadowMap : register(t0, space1);
SamplerState shadowSampler : register(s0, space1);

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

static const float4x4 biasMat = float4x4(
	0.5, 0.0, 0.0, 0.5,
	0.0, 0.5, 0.0, 0.5,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0 );

VSOutput vert(VSInput input)
{
    VSOutput output;
    output.color = input.color;
    output.normal = input.normal;

    float4 worldPos = mul(pushConstants.model, float4(input.pos, 1.0));
    float4 viewPos = mul(view, worldPos);
    output.pos = mul(projection, viewPos);
    
    output.normal = mul((float3x3)pushConstants.model, input.normal);
    output.lightDir = normalize(lightPos - worldPos.xyz);
    
    float slopeBias = clamp(max(0.005, 0.01 * (1.0 - dot(output.normal, output.lightDir))), 0.0, 0.02);
    float4 shadowWorldPos = mul(pushConstants.model, float4(input.pos - input.normal * slopeBias, 1.0));
    float4 shadowCoord = mul(lightProjection, mul(lightView, shadowWorldPos));
    output.shadowCoord = mul(biasMat, shadowCoord);

    return output;
}

float SampleShadow(float4 shadowCoord, float2 offset = float2(0, 0))
{
    float shadow = 1.0f;
    float2 uv = shadowCoord.xy / shadowCoord.w;
    float shadowDepth = shadowMap.Sample(shadowSampler, uv + offset).r;

    float bias = shadowBias;
    if (shadowDepth < shadowCoord.z - bias)
    {
        shadow = 0.0f;
    }

    return shadow;
}

float filterPCF(float4 shadowCoord)
{
    int2 texDim;
    shadowMap.GetDimensions(texDim.x, texDim.y);

    float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += SampleShadow(shadowCoord, float2(dx*x, dy*y));
			count++;
		}

	}
	return shadowFactor / count;
}

float4 frag(
    [[vk::location(0)]] float3 normal : NORMAL0,
    [[vk::location(1)]] float4 color : COLOR0,
    [[vk::location(2)]] float4 shadowCoord : TEXCOORD1,
    [[vk::location(3)]] float3 lightDir : TEXCOORD2) : SV_TARGET
{
    float ambient = 0.1;
    
    float visibility = (enablePCF == 0.0f) ? SampleShadow(shadowCoord) : filterPCF(shadowCoord);
    visibility = 0.5 + 0.5 * visibility;

    float3 N = normalize(normal);
    float3 L = normalize(lightDir);

    float3 diffuse = max(dot(N, L), ambient) * color.rgb;

    return float4(diffuse * visibility, 1.0);
}