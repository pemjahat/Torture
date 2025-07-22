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
    uint vertexOffset;
    
    float3 extentsBound;
    uint indexOffset;
    
    float4x4 meshTransform; //Per-mesh transform
};

#endif