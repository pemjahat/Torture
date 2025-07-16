#pragma once

#ifdef HLSL
typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef float4x4 XMFLOAT4X4;
typedef uint UINT;
#else
using namespace DirectX;
#endif

struct SceneConstantBuffer
{
    XMFLOAT4X4 World;
    XMFLOAT4X4 WorldView;
    XMFLOAT4X4 WorldViewProj;
    XMVECTOR ProjToWorld;
    XMVECTOR CamPosition;
    XMFLOAT2 InvTextureSize;
    XMFLOAT2 HiZDimension;
};

struct LightData
{
    XMFLOAT3 direction;
    float intensity;
    XMFLOAT3 color;
    float padding;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};