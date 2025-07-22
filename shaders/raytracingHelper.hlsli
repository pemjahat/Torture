#pragma once

#define HLSL
#include "HLSLCompatible.h"

    struct Ray
    {
        float3 origin;
        float3 direction;
    };
    
    // Retrieve hit world position
    float3 HitWorldPosition()
    {
        return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    }
    
    float2 BarycentricLerp(in float2 v0, in float2 v1, in float2 v2, in float3 barycentrics)
    {
        return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    }
    float3 BarycentricLerp(in float3 v0, in float3 v1, in float3 v2, in float3 barycentrics)
    {
        return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    }
    float4 BarycentricLerp(in float4 v0, in float4 v1, in float4 v2, in float3 barycentrics)
    {
        return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    }
    MeshVertex BarycentricLerp(in MeshVertex v0, in MeshVertex v1, in MeshVertex v2, in float3 barycentrics)
    {
        MeshVertex vtx;
        vtx.Position = BarycentricLerp(v0.Position, v1.Position, v2.Position, barycentrics);
        vtx.Normal = BarycentricLerp(v0.Normal, v1.Normal, v2.Normal, barycentrics);
        vtx.Color = BarycentricLerp(v0.Color, v1.Color, v2.Color, barycentrics);
        vtx.Tangent = BarycentricLerp(v0.Tangent, v1.Tangent, v2.Tangent, barycentrics);
        vtx.Uv = BarycentricLerp(v0.Uv, v1.Uv, v2.Uv, barycentrics);
        return vtx;
    }
    
    // Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
    inline Ray GenerateCameraRay(uint2 index, in float3 cameraPosition, in float4x4 projectionToWorld)
    {
        float2 xy = index + 0.5f; // center in the middle of the pixel.
        float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

        // Invert Y for DirectX-style coordinates.
        screenPos.y = -screenPos.y;

        // Unproject the pixel coordinate into a world positon.
        float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);
        world.xyz /= world.w;

        Ray ray;
        ray.origin = cameraPosition;
        ray.direction = normalize(world.xyz - ray.origin);

        return ray;
    }
    
    // Texture coordinates on a horizontal plane.
    float2 TexCoords(in float3 position)
    {
        return position.xz;
    }

    // Calculate ray differentials.
    void CalculateRayDifferentials(out float2 ddx_uv, out float2 ddy_uv, out float3 ddx_pos, out float3 ddy_pos,
        in float2 uv, in float3 hitPosition, in float3 surfaceNormal, in float3 cameraPosition, in float4x4 projectionToWorld)
    {
        // Compute ray differentials by intersecting the tangent plane to the  surface.
        Ray ddx = GenerateCameraRay(DispatchRaysIndex().xy + uint2(1, 0), cameraPosition, projectionToWorld);
        Ray ddy = GenerateCameraRay(DispatchRaysIndex().xy + uint2(0, 1), cameraPosition, projectionToWorld);

        // Compute ray differentials.
        ddx_pos = ddx.origin - ddx.direction * dot(ddx.origin - hitPosition, surfaceNormal) / dot(ddx.direction, surfaceNormal);
        ddy_pos = ddy.origin - ddy.direction * dot(ddy.origin - hitPosition, surfaceNormal) / dot(ddy.direction, surfaceNormal);

        // Calculate texture sampling footprint.
        ddx_uv = TexCoords(ddx_pos) - uv;
        ddy_uv = TexCoords(ddy_pos) - uv;
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