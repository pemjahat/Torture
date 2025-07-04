#ifndef _COMMON_HLSL_
#define _COMMON_HLSL_

struct SceneConstantBuffer
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    
    float2 InvTextureSize;
    float2 HiZDimension;

    float4 padding[3];
};

struct LightData
{
    float3 direction;
    float intensity;
    float3 color;
    float padded;
};

struct ModelConstants
{
    uint meshIndex; // index for mesh structured buffer
    uint materialIndex; // index for material structured buffer
};

struct MeshData
{
    float3 centerBound;
    float padBound1;
    
    float3 extentsBound;
    float padBound2;
    
    float4x4 meshTransform; //Per-mesh transform
};

struct MaterialData
{
    int useVertexColor;
    int useTangent; //  1 if tangent available, 0 use Mikktspace
    float metallicFactor;
    float roughnessFactor;
    
    // >=0 if available
    int albedoTextureIndex;
    int metallicTextureIndex;
    int normalTextureIndex;
    float paddedMat;
    
    float4 baseColorFactor;
};

#endif