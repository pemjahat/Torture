#ifndef _HLSL_COMPATIBLE_
#define _HLSL_COMPATIBLE_

#ifdef HLSL
typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef row_major float4x4 XMMATRIX;
typedef uint UINT;
typedef uint2 XMUINT2;
#else
using namespace DirectX;
#endif

#define MAX_ANYHIT_DEPTH 2
#define MAX_RECURSION_DEPTH 1   // primary ray + reflection + shadows

struct SceneConstantBuffer
{
    XMMATRIX World;
    XMMATRIX WorldView;
    XMMATRIX WorldViewProj;
    XMMATRIX ProjToWorld;
    XMVECTOR CamPosition;
    XMFLOAT2 InvTextureSize;
    XMUINT2 RayDimension;
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
    float reflectanceCoeff;
    float diffuseCoeff;
    float specularCoeff;
    float specularPower;
};

struct InstanceInfo
{
    UINT VtxOffset;
    UINT IdxOffsetByBytes;  // Accessing byte buffer
    UINT MaterialIdx;
    UINT UseTangent;
    UINT UseVertexColor;
};

struct MaterialData
{
    XMFLOAT4 baseColorFactor;

    float metallicFactor;
    float roughnessFactor;

    int albedoTextureIndex;
    int metallicTextureIndex;   // Metallic - Roughness - 
    int normalTextureIndex;
    float alphaCutoff;

    int albedoViewTextureIndex;
    int metallicViewTextureIndex;
    int normalViewTextureIndex;
};

struct MeshStructuredBuffer
{
    XMFLOAT3 centerBound;
    int useVertexColor;

    XMFLOAT3 extentsBound;
    int useTangent; //  1 if tangent available, 0 use Mikktspace

    XMMATRIX meshTransform;
};

struct Vertex   // Test
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
};

struct MeshVertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
    XMFLOAT4 Tangent;   // tangent (x, y, z) handedness (w)
    XMFLOAT2 Uv;
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