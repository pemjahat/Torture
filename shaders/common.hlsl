#ifndef _COMMON_HLSL_
#define _COMMON_HLSL_

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
    float alphaCutoff;
    
    float4 baseColorFactor;
};

#endif