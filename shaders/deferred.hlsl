// basic.hlsl (depth only)
#include "common.hlsl"
#define HLSL
#include "HLSLCompatible.h"

struct DeferredConstant
{
    uint albedoIndex;
    uint normalIndex;
    uint materialIndex;
};

ConstantBuffer<LightData> lightCB : register(b0);
ConstantBuffer<DeferredConstant> deferredCB : register(b1);

Texture2D materialTex[] : register(t0, space1); // bindless for material (share desc heap)
Texture2D<float4> rtBuffer : register(t0);

SamplerState g_sampler : register(s0);

float4 PSMain(float4 position : SV_Position, float2 texCoord : TEXCOORD) : SV_Target0
{
    float3 albedo = materialTex[NonUniformResourceIndex(deferredCB.albedoIndex)].Sample(g_sampler, texCoord).rgb;
    float3 normalMap = materialTex[NonUniformResourceIndex(deferredCB.normalIndex)].Sample(g_sampler, texCoord).rgb;
    float4 material = materialTex[NonUniformResourceIndex(deferredCB.materialIndex)].Sample(g_sampler, texCoord);
    
    // Metallic - Roughness
    float metallic = material.b;
    float roughness = material.g;
    
    // Lighting calculation
    float3 lightDir = normalize(-lightCB.direction);
    float NdotL = max(dot(normalMap, lightDir), 0.0);
 
    float shadow = rtBuffer.Sample(g_sampler, texCoord).r;
    float3 outColor = lightCB.ambient.rgb + albedo;
    outColor += shadow * (albedo * lightCB.color.rgb * lightCB.intensity * NdotL);
 
    return float4(outColor, 1.f);
}