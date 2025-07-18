#define HLSL
#include "HLSLCompatible.h"
#include "raytracingHelper.hlsli"

RaytracingAccelerationStructure scene : register(t0);
ByteAddressBuffer Indices : register(t1);
StructuredBuffer<Vertex> Vertices : register(t2);

RWTexture2D<float4> renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<LightData> lightCB : register(b1);
ConstantBuffer<PrimitiveConstantBuffer> matCB : register(b2);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

// HitGroupIdx = RayContribToHitGroupIndex + GeomIndex * MultiplerGeomContribToHitGroupIndex
float4 TraceRadianceRay(Ray ray, UINT currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= MAX_RECURSION_DEPTH)
    {
        return float4(0, 0, 0, 0);
    }
        
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
        
    RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1 };
    TraceRay(
        scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
        0,
        TraceRayParameters::MissShader::Offset[RayType::Radiance],
        rayDesc,
        rayPayload);

    return rayPayload.color;
}
    
bool TraceShadowRay(Ray ray, UINT currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= MAX_RECURSION_DEPTH)
    {
        return false;
    }
        
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin + ray.direction * 0.1f;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
        
    // Init value is true since closest hit + any hit is skipped
    // Only if miss called, will set to false
    ShadowRayPayload shadowPayload = { true };
    TraceRay(
        scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_FORCE_OPAQUE |
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // Skip closest hit shader
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Shadow],
        0,
        TraceRayParameters::MissShader::Offset[RayType::Shadow],
        rayDesc,
        shadowPayload);

    return shadowPayload.hit;
}
    
float4 CalculatePhongLighting(float4 albedo, float3 normal, bool isInShadow, float diffuseCoeff = 1.f, float specularCoeff = 1.f, float specularPower = 50.f)
{
    float3 hitPosition = HitWorldPosition();
    float shadowFactor = isInShadow ? InShadowRadiance : 1.0f;
    float3 incidentLightRay = normalize(-lightCB.direction);
        
    // Diffuse
    float4 lightDiffuseColor = lightCB.color;
    float Kd = CalculateDiffuseCoefficient(incidentLightRay, normal);
    float4 diffuseColor = shadowFactor * diffuseCoeff * Kd * lightDiffuseColor * albedo;
        
    // Specular
    float4 specularColor = float4(0, 0, 0, 0);
    if (!isInShadow)
    {
        float4 lightSpecularColor = float4(1, 1, 1, 1);
        float4 Ks = CalculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
        float4 specularColor = specularCoeff * Ks * lightSpecularColor;
    }
        
    // Ambient
    float4 ambientColor = lightCB.ambient * albedo;
        
    return ambientColor + diffuseColor + specularColor;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, sceneCB.CamPosition.xyz, sceneCB.ProjToWorld);
    
    // cast ray into scene and retrieve shaded color
    UINT currentRecursionDepth = 0;
    float4 color = TraceRadianceRay(ray, currentRecursionDepth);
        
    // Write into output
    renderTarget[DispatchRaysIndex().xy] = color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    // Get base index of triangle's first 16 bit index
    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;
    
    const uint3 indices = LoadIndices(baseIndex, Indices);
        
    float3 triangleNormal = Vertices[indices[0]].normal;
    
    // Shadow
    float3 hitPosition = HitWorldPosition();
    Ray shadowRay = { hitPosition, normalize(-lightCB.direction) };
    bool shadowRayHit = TraceShadowRay(shadowRay, payload.recursionDepth);
     
    // Reflection
    float4 reflectedColor = float4(0, 0, 0, 0);
    if (matCB.reflectanceCoeff > 0.001 && GeometryIndex() == 0)
    {
        Ray reflectionRay = { HitWorldPosition(), reflect(WorldRayDirection(), triangleNormal) };
        float4 reflectionColor = TraceRadianceRay(reflectionRay, payload.recursionDepth);

        float3 fresnelR = FresnelReflectionShlick(WorldRayDirection(), triangleNormal, matCB.albedo.xyz);
        reflectedColor = matCB.reflectanceCoeff * float4(fresnelR, 1.f) * reflectionColor;
    }
        
    // Final Color
    float4 phongColor = CalculatePhongLighting(matCB.albedo, triangleNormal, shadowRayHit, matCB.diffuseCoeff, matCB.specularCoeff, matCB.specularPower);        
    float4 color = phongColor + reflectedColor;

    // Visibility falloff
    float t = RayTCurrent();
    color = lerp(color, BackgroundColor, (1.0 - exp(-0.000002 * t * t * t)));
        
    payload.color = color;
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float4 backgroundColor = float4(BackgroundColor);
    payload.color = backgroundColor;
}
    
[shader("miss")]
void MyMissShader_Shadow(inout ShadowRayPayload payload)
{
    payload.hit = false;
}