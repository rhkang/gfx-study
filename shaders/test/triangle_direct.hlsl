static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

static const float4 colors[3] = {
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0)
};

struct v2f
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

v2f vert(uint index : SV_VertexID) {
    v2f output;
    output.pos = float4(positions[index], 0.0, 1.0);
    output.color = colors[index];
    return output;
}

float4 frag(v2f input) : SV_TARGET
{
    return input.color;
}