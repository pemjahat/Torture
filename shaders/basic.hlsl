// basic.hlsl (depth only)
#include "common.hlsl"
#define HLSL
#include "HLSLCompatible.h"

ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<ModelConstants> modelConstants : register(b2);
Texture2D materialTex[] : register(t0, space1);
    StructuredBuffer<MeshStructuredBuffer> meshData : register(t0);
    StructuredBuffer<MaterialData> materialData : register(t1);

    SamplerState g_sampler : register(s0);
    
    struct VSInput
    {
        float3 position : POSITION;
        float2 uv : TEXCOORD;
    };
    
    struct VSOutput
    {
        float4 position : SV_Position;
        float2 uv : TexCoord0;
    };

    VSOutput VSMain(VSInput input)
    {
        MeshStructuredBuffer mesh = meshData[modelConstants.meshIndex];
    
        VSOutput output;
        output.position = float4(input.position.xyz, 1.f);
        output.position = mul(output.position, mesh.meshTransform);
        output.position = mul(output.position, sceneCB.WorldViewProj);
        output.uv = input.uv;
        
        return output;
    }
    
    void PSMain(VSOutput vsOutput)
    {
        MaterialData material = materialData[modelConstants.materialIndex];
        
        float4 albedo = materialTex[NonUniformResourceIndex(material.albedoViewTextureIndex)].Sample(g_sampler, vsOutput.uv);
        if (albedo.a < material.alphaCutoff)
            discard;
    }
