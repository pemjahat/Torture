// basic.hlsl (depth only)
#include "common.hlsl"

ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<ModelConstants> modelConstants : register(b2);
StructuredBuffer<MeshData> meshData : register(t0);

struct VSInput
{
    float3 position : POSITION;
};

float4 VSMain(VSInput input) : SV_Position
{
    MeshData mesh = meshData[modelConstants.meshIndex];
    
    float4 pos = float4(input.position.xyz, 1.f);
    pos = mul(pos, mesh.meshTransform);
    pos = mul(pos, sceneCB.WorldViewProj);
    return pos;
}