// basic.hlsl
struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = input.position;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //return input.color;
    return g_texture.Sample(g_sampler, input.uv);
}