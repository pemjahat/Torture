// basic_color.hlsl
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    float4 padding[4];
};

cbuffer LightData : register(b1)
{
    float3 direction;
    float intensity;
    float3 color;
    float padded;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);

SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    
    output.position = mul(float4(input.position, 1.f), WorldViewProj);
    output.worldPos = input.position;
    output.normal = input.normal;
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 albedo = albedoTexture.Sample(g_sampler, input.uv);
    float3 normalMap = normalTexture.Sample(g_sampler, input.uv); // normal map in tangent space
    normalMap = normalMap * 2.f - 1.f;
    normalMap = normalize(normalMap);
    
    // Compute TBN using Mikktspace (http://www.thetenthplanet.de/archives/1180)
    float3 N = normalize(input.normal);
    float3 dPdx = ddx(input.worldPos);
    float3 dPdy = ddy(input.worldPos);
    float2 dUVdx = ddx(input.uv);
    float2 dUVdy = ddy(input.uv);

    // Solve for T and B
    float3 dp2perp = cross(dPdy, N);
    float3 dp1perp = cross(N, dPdx);
    float3 T = dp2perp * dUVdx.x + dp1perp * dUVdy.x;
    float3 B = dp2perp * dUVdx.y + dp1perp * dUVdy.y;
    
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    T = T * invmax;
    B = B * invmax;

    // Orthogonalize T (Gram-Schmidt)
    T = normalize(T - dot(T, N) * N);
    B = normalize(cross(N, T)); // Ensure correct handedness
    float3x3 TBN = float3x3(T, B, N);

    // Transform normal to world space
    float3 worldNormal = normalize(mul(normalMap, TBN));

    // Lighting calculation
    float3 lightDir = normalize(-direction);
    float NdotL = max(dot(worldNormal, lightDir), 0.0);
    
    float3 lighting = albedo.rgb * color * intensity * NdotL;
    return float4(lighting, albedo.a);
}