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

// Retrieve hit world position
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve 3 indices (16 bits) from byte address buffer
uint3 LoadIndices(uint offsetBytes)
{
    uint3 indices;
    
    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
    
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }
    
    return indices;
}

// Retrieve attribute of hit position interpolated from vertex attribute using hit barrycentric
float3 HitAttribute(float3 vertexAtrribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAtrribute[0] +
        attr.barycentrics.x * (vertexAtrribute[1] - vertexAtrribute[0]) +
        attr.barycentrics.y * (vertexAtrribute[2] - vertexAtrribute[0]);
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    
    
    // Get base index of triangle's first 16 bit index
    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;
    
    const uint3 indices = LoadIndices(baseIndex);
        
        float3 triangleNormal = Vertices[indices[0]].normal;
    
        // Shadow
        float3 hitPosition = HitWorldPosition();
        Ray shadowRay = { hitPosition, normalize(-lightCB.direction) };
        bool shadowRayHit = TraceShadowRay(shadowRay, payload.recursionDepth);

        // Reflection
        float4 reflectedColor = float4(0, 0, 0, 0);
        if (matCB.reflectanceCoeff > 0.001)
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
    //// Retrieve vertex
    //float3 vertexNormals[3] =
    //{
    //    Vertices[indices[0]].normal,
    //    Vertices[indices[1]].normal,
    //    Vertices[indices[2]].normal,
    //};
    
    //float3 triangleNormal = HitAttribute(vertexNormals, attr);
    
    //float3 lightDir = normalize(-lightCB.direction);
    //float NdotL = max(dot(triangleNormal, lightDir), 0.0);
    
    //float3 lighting = lightCB.ambient.rgb + (float3(1.f, 0.f, 0.f) * lightCB.color.rgb * lightCB.intensity * NdotL);
    
    //// Compute triangle normal
    //payload.color = float4(lighting, 1.f);
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