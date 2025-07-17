#ifndef _HLSL_COMPATIBLE_
#define _HLSL_COMPATIBLE_

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

#define MAX_RECURSION_DEPTH 3   // primary ray + reflection + shadows

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

struct PrimitiveConstantBuffer
{
    XMFLOAT4 albedo;
    float reflectionCoeff;
    float diffuseCoeff;
    float specularCoeff;
    float specularPower;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};

struct RayPayload
{
    XMFLOAT4 color;
    UINT recursionDepth;
};

struct ShadowRayPayload
{
    bool hit;
};

namespace RayType
{
    enum Enum {
        Radiance = 0,
        Shadow,
        Count
    };
}

namespace TraceRayParameters
{
    static const UINT InstanceMask = ~0;    // everything visible
    namespace HitGroup
    {
        static const UINT Offset[RayType::Count] =
        {
            0,  // Radiance
            1  // Shadow
        };
        static const UINT GeometryStride = RayType::Count;
    }
    namespace MissShader
    {
        static const UINT Offset[RayType::Count] =
        {
            0,  // Radiance
            1   // Shadow
        };        
    }
}

// From: http://blog.selfshadow.com/publications/s2015-shading-course/hoffman/s2015_pbs_physics_math_slides.pdf
static const XMFLOAT4 ChromiumReflectance = XMFLOAT4(0.549f, 0.556f, 0.554f, 1.0f);

static const XMFLOAT4 BackgroundColor = XMFLOAT4(0.8f, 0.9f, 1.0f, 1.0f);
static const float InShadowRadiance = 0.35f;

#endif