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
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            TraceRayParameters::InstanceMask,
            TraceRayParameters::HitGroup::Offset[RayType::Radiance],
            TraceRayParameters::HitGroup::GeometryStride,
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
        rayDesc.Origin = ray.origin;
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
            RAY_FLAG_FORCE_OPAQUE |             // Skip any hit shader
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,   // Skip closest hit shader
            TraceRayParameters::InstanceMask,
            TraceRayParameters::HitGroup::Offset[RayType::Shadow],
            TraceRayParameters::HitGroup::GeometryStride,
            TraceRayParameters::MissShader::Offset[RayType::Shadow],
            rayDesc,
            shadowPayload);

        return shadowPayload.hit;
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