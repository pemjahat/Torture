// basic.hlsl (depth only)
#include "common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 material : SV_Target2;
};

ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<ModelConstants> modelConstants : register(b1);

Texture2D materialTex[] : register(t0, space1); // bindless for material (share desc heap)
StructuredBuffer<MeshData> meshData : register(t0);
StructuredBuffer<MaterialData> materialData : register(t1);

SamplerState g_sampler : register(s0);

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    MeshData mesh = meshData[modelConstants.meshIndex];
    
    // Apply mesh transform
    float4 pos = float4(input.position, 1.f);
    pos = mul(pos, mesh.meshTransform);
    
    output.worldPos = pos;
    output.position = mul(pos, sceneCB.WorldViewProj);
    
    float4 norm = float4(input.normal, 0.f);
    output.normal = normalize(mul(norm, sceneCB.World).xyz);
    
    float4 tangent = float4(input.tangent.xyz, 0.f);
    output.tangent = float4(normalize(mul(tangent, sceneCB.World).xyz), input.tangent.w);
    
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

PSOutput PSMain(VSOutput input)
{
    MaterialData material = materialData[modelConstants.materialIndex];
    
    //
    // Normal
    //
    float3x3 TBN;
    if (material.useTangent)
    {
        float3 T = normalize(input.tangent.xyz);
        float3 N = normalize(input.normal);
        T = normalize(T - dot(T, N) * N); /// Orthogonalize
        float3 B = normalize(cross(N, T) * input.tangent.w);
        TBN = float3x3(T, B, N);
    }
    // Compute TBN using Mikktspace (http://www.thetenthplanet.de/archives/1180)
    else
    {
        float3 N = normalize(input.normal);
        float3 dPdx = ddx(input.worldPos.xyz);
        float3 dPdy = ddy(input.worldPos.xyz);
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
        TBN = float3x3(T, B, N);
    }
    
    float3 worldNormal = normalize(input.normal);
    if (material.normalTextureIndex >= 0)
    {
        float3 normalMap = materialTex[NonUniformResourceIndex(material.normalTextureIndex)].Sample(g_sampler, input.uv).rgb;
        normalMap = normalMap * 2.f - 1.f;
        normalMap = normalize(normalMap);
        
        worldNormal = normalize(mul(normalMap, TBN));
    }

    //
    // Albedo
    //
    float3 albedo = material.baseColorFactor.rgb;
    if (material.useVertexColor)
    {
        albedo = input.color.rgb;
    }
    else if (material.albedoTextureIndex >= 0)
    {
        albedo *= materialTex[NonUniformResourceIndex(material.albedoTextureIndex)].Sample(g_sampler, input.uv).rgb;
    }
    
    //
    // Metallic - Roughness
    //
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if (material.metallicTextureIndex >= 0)
    {
        float3 matSample = materialTex[NonUniformResourceIndex(material.metallicTextureIndex)].Sample(g_sampler, input.uv).rgb;
        metallic = matSample.b;
        roughness = matSample.g;
    }
    
    PSOutput output;
    output.albedo = float4(albedo.rgb, 0.f);
    output.normal = float4(worldNormal.rgb, 0.f);
    output.material = float4(metallic, roughness, 0.f, 0.f);
    
    return output;
}