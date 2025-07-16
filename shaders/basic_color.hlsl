#include "common.hlsl"
#define HLSL
#include "HLSLCompatible.h"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<LightData> lightCB : register(b1);
ConstantBuffer<ModelConstants> modelConstants : register(b2);

Texture2D materialTex[] : register(t0, space1); // bindless for material (share desc heap)
StructuredBuffer<MeshData> meshData : register(t0);
StructuredBuffer<MaterialData> materialData : register(t1);

SamplerState g_sampler : register(s0);

//bool IsAABBVisible(float3 center, float3 extents, float4x4 worldViewProj)
//{
//    // transform aabb to clip space
//    float3 minBound = center - extents;
//    float3 maxBound = center + extents;
//    float3 corners[8];
//    corners[0] = float3(minBound.x, minBound.y, minBound.z);
//    corners[1] = float3(maxBound.x, minBound.y, minBound.z);
//    corners[2] = float3(minBound.x, maxBound.y, minBound.z);
//    corners[3] = float3(maxBound.x, maxBound.y, minBound.z);
//    corners[4] = float3(minBound.x, minBound.y, maxBound.z);
//    corners[5] = float3(maxBound.x, minBound.y, maxBound.z);
//    corners[6] = float3(minBound.x, maxBound.y, maxBound.z);
//    corners[7] = float3(maxBound.x, maxBound.y, maxBound.z);
    
//    float2 minUV = float2(1.f, 1.f);
//    float2 maxUV = float2(0.f, 0.f);
//    float minDepth = 1.f;   // far
//    for (int i = 0; i < 8; ++i)
//    {
//        // Transform to clip space
//        float4 clipPos = mul(float4(corners[i], 1.f), worldViewProj);
//        clipPos /= clipPos.w;   // perspective divide
        
//        // Compute screen space uv
//        float2 uv = float2(clipPos.x * 0.5 + 0.5, 1.f - (clipPos.y * 0.5 + 0.5));
//        minUV = min(minUV, uv);
//        maxUV = max(maxUV, uv);
//        minDepth = min(minDepth, clipPos.z);    // closest depth (0 - near, 1 - far)
//    }
    
//    // clamp
//    minUV = max(minUV, 0.f);
//    maxUV = min(maxUV, 1.f);
    
//    // Mip level based on aabb screen size
//    float2 aabbSize = (maxUV - minUV) * HiZDimension;
//    float mipLevel = ceil(log2(max(aabbSize.x, aabbSize.y)));
    
//    // Sample hiz texture at conservative postion (center of aabb)
//    float2 sampleUV = (minUV + maxUV) * 0.5f;
//    float hizDepth = HiZTex.SampleLevel(g_sampler, sampleUV, mipLevel);
    
//    // Occlusio test: aabb occluded if closes depth behind hiz depth
//    return minDepth <= hizDepth;
//}

PSInput VSMain(VSInput input)
{
    PSInput output;
    
    MeshData mesh = meshData[modelConstants.meshIndex];
    
    // Apply mesh transform
    float4 pos = float4(input.position, 1.f);
    pos = mul(pos, mesh.meshTransform);
    
    //if (!IsAABBVisible(centerBound, extentsBound, WorldViewProj))
    //{
    //    // Out degenerate position cull vertex
    //    output.position = float4(0, 0, 0, 0);
    //    output.worldPos = float4(0, 0, 0, 0);
    //    output.normal = float3(0, 0, 0);
    //    output.tangent = float4(0, 0, 0, 0);
    //    output.uv = float2(0, 0);
    //    return output;
    //}
    
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

float4 PSMain(PSInput input) : SV_TARGET
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
    //float diffuseFactor = 1.f - metallic;
    
    // Lighting calculation
    float3 lightDir = normalize(-lightCB.direction);
    float NdotL = max(dot(worldNormal, lightDir), 0.0);
    
    float3 lighting = albedo * lightCB.color * lightCB.intensity * NdotL;
    return float4(lighting, 1.f);
}