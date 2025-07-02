// basic.hlsl (depth only)
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    
    float2 InvTextureSize;
    float2 HiZDimension;
    
    float4 padding[3];
};

cbuffer MaterialData : register(b1)
{
    int useVertexColor;
    int useTangent; //  1 if tangent available, 0 use Mikktspace
    float metallicFactor;
    float roughnessFactor;
    
    int hasAlbedoMap;
    int hasMetallicRoughnessMap; // 1 if metallic roughness map available
    int hasNormalMap; // 1 if normal map availabe
    float paddedMat;
    
    float3 centerBound;
    float padBound1;
    float3 extentsBound;
    float padBound2;
    
    float4 baseColorFactor;
    float4x4 meshTransform; //Per-mesh transform
};

struct VSInput
{
    float3 position : POSITION;
};

float4 VSMain(VSInput input) : SV_Position
{
    float4 pos = float4(input.position.xyz, 1.f);
    pos = mul(pos, meshTransform);
    pos = mul(pos, WorldViewProj);
    return pos;
}