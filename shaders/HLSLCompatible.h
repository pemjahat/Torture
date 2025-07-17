#pragma once

#ifdef HLSL
typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef row_major float4x4 XMMATRIX;
typedef uint UINT;
#else
using namespace DirectX;
#endif

struct SceneConstantBuffer
{
    XMMATRIX World;
    XMMATRIX WorldView;
    XMMATRIX WorldViewProj;
    XMMATRIX ProjToWorld;
    XMVECTOR CamPosition;
    XMFLOAT2 InvTextureSize;
    XMFLOAT2 HiZDimension;
};

struct LightData
{
    XMFLOAT3 direction;
    float intensity;
    XMVECTOR color;
    XMVECTOR ambient;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};