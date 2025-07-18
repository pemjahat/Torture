#pragma once

#define HLSL
#include "HLSLCompatible.h"

    struct Ray
    {
        float3 origin;
        float3 direction;
    };
    
    Ray GenerateCameraRay(uint2 index, in float3 camPosition, in float4x4 projToWorld)
    {
        float2 xy = index + 0.5f;
        float2 screenPos = xy / DispatchRaysDimensions().xy * 2.f - 1.f;
        
        // DX coordinate
        screenPos.y = -screenPos.y;
        
        // unproject screen pos into ray
        float4 world = mul(float4(screenPos, 0, 1), projToWorld);
        
        Ray ray;
        world.xyz /= world.w;
        ray.origin = camPosition;
        ray.direction = normalize(world.xyz - ray.origin);
        
        return ray;
    }
    
    // Retrieve hit world position
    float3 HitWorldPosition()
    {
        return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    }
    
    // Retrieve attribute of hit position interpolated from vertex attribute using hit barrycentric
    float3 HitAttribute(float3 vertexAtrribute[3], BuiltInTriangleIntersectionAttributes attr)
    {
        return vertexAtrribute[0] +
            attr.barycentrics.x * (vertexAtrribute[1] - vertexAtrribute[0]) +
            attr.barycentrics.y * (vertexAtrribute[2] - vertexAtrribute[0]);
    }

    // Retrieve 3 indices (16 bits) from byte address buffer
    uint3 LoadIndices(uint offsetBytes, ByteAddressBuffer Indices)
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
    
    float3 FresnelReflectionShlick(in float3 I, in float3 N, in float3 f0)
    {
        float cosi = saturate(dot(-I, N));
        return f0 + (1 - f0) * pow(1 - cosi, 5);
    }
    
    //
    // Lighting
    //
    float CalculateDiffuseCoefficient(in float3 incidentLightRay, in float3 normal)
    {
        float NDotL = saturate(dot(-incidentLightRay, normal));
        return NDotL;
    }
    
    float4 CalculateSpecularCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal, in float specularPower)
    {
        float3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
        return pow(saturate(dot(reflectedLightRay, normalize(-WorldRayDirection()))), specularPower);
    }