#define HLSL
#include "HLSLCompatible.h"
#include "raytracingHelper.hlsli"

    RaytracingAccelerationStructure scene : register(t0);
    ByteAddressBuffer Indices : register(t1);
    StructuredBuffer<MeshVertex> Vertices : register(t2);
    StructuredBuffer<InstanceInfo> instanceData : register(t3);
    StructuredBuffer<MaterialData> materialData : register(t4);

    Texture2D materialTex[] : register(t0, space1); // bindless for material (share desc heap)
    
    RWTexture2D<float4> renderTarget : register(u0);
    ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
    ConstantBuffer<LightData> lightCB : register(b1);
    
    SamplerState g_sampler : register(s0);
    
    typedef BuiltInTriangleIntersectionAttributes MyAttributes;

    MeshVertex GetHitSurface(in MyAttributes attr, in uint instanceIdx)
    {
        float3 barycentric = float3(1.f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
        
        InstanceInfo instInfo = instanceData[instanceIdx];
        
        uint primIdx = PrimitiveIndex();  // Prim within geom
        
        uint indexByteOffset = instInfo.IdxOffsetByBytes + (primIdx * 3) * 4;
        uint idx0 = Indices.Load(indexByteOffset + 0);
        uint idx1 = Indices.Load(indexByteOffset + 4);
        uint idx2 = Indices.Load(indexByteOffset + 8);
        
        MeshVertex vtx0 = Vertices[idx0 + instInfo.VtxOffset];
        MeshVertex vtx1 = Vertices[idx1 + instInfo.VtxOffset];
        MeshVertex vtx2 = Vertices[idx2 + instInfo.VtxOffset];
        
        return BarycentricLerp(vtx0, vtx1, vtx2, barycentric);
    }
    
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
        
        uint rayTraceFlag = RAY_FLAG_FORCE_OPAQUE;
        //uint rayTraceFlag = 0;
        //if (currentRayRecursionDepth >= MAX_ANYHIT_DEPTH)
        //    rayTraceFlag = RAY_FLAG_FORCE_OPAQUE;
        
        RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1 };
        TraceRay(
        scene,
        rayTraceFlag,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
        //RayType::Count,
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
        
        uint traceRayFlag = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        //uint traceRayFlag = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        //if (currentRayRecursionDepth >= MAX_ANYHIT_DEPTH)
        //    traceRayFlag = RAY_FLAG_FORCE_OPAQUE;
        
    // Init value is true since closest hit + any hit is skipped
    // Only if miss called, will set to false
        ShadowRayPayload shadowPayload = { true };
        TraceRay(
        scene,
        traceRayFlag,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Shadow],
        //RayType::Count,
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
        //uint seed = InstanceIndex();
        //payload.color = float4(QuickRandomFloat(seed), QuickRandomFloat(seed), QuickRandomFloat(seed), 1.f);
        
        const MeshVertex hitSurface = GetHitSurface(attr, InstanceIndex());
        InstanceInfo instInfo = instanceData[InstanceIndex()];
        const MaterialData material = materialData[instInfo.MaterialIdx];

        // Normal
        float3x3 TBN;
        float3 N = normalize(hitSurface.Normal);
        
        if (instInfo.UseTangent)
        {
            float3 T = normalize(hitSurface.Tangent.xyz);
            T = normalize(T - dot(T, N) * N); /// Orthogonalize
            float3 B = normalize(cross(N, T) * hitSurface.Tangent.w);
            TBN = float3x3(T, B, N);
        }
        // Compute TBN using Mikktspace (http://www.thetenthplanet.de/archives/1180)
        else
        {
            float3 dPdx, dPdy;
            float2 dUVdx, dUVdy;
            CalculateRayDifferentials(dUVdx, dUVdy, dPdx, dPdy, hitSurface.Uv, hitSurface.Position, N, sceneCB.CamPosition.xyz, sceneCB.ProjToWorld);
            
            // Solve for T and B
            float3 dp2perp = cross(dPdy, N);
            float3 dp1perp = cross(N, dPdx);
            float3 T = dp2perp * dUVdx.x + dp1perp * dUVdy.x;
            float3 B = dp2perp * dUVdx.y + dp1perp * dUVdy.y;
    
            float invmax = rsqrt(max(dot(T, T), dot(B, B)));
            T = T * invmax;
            B = B * invmax;
        
            // Orthogonalize T (Gram-Schmidt)
            T = normalize(T - dot(T, N) * N);
            B = normalize(cross(N, T)); // Ensure correct handedness
            TBN = float3x3(T, B, N);
        }
        
        float3 worldNormal = N;
        if (material.normalViewTextureIndex >= 0)
        {
            float3 normalMap = materialTex[NonUniformResourceIndex(material.normalViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0).rgb;
            normalMap = normalMap * 2.f - 1.f;
            normalMap = normalize(normalMap);
        
            worldNormal = normalize(mul(normalMap, TBN));
        }
        
        //
        // Albedo
        //
        float4 baseColor = material.baseColorFactor;
        if (instInfo.UseVertexColor)
        {
            baseColor = hitSurface.Color;
        }
        else if (material.albedoViewTextureIndex >= 0)
        {
            baseColor *= materialTex[NonUniformResourceIndex(material.albedoViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0);
        }
        
        //
        // Metallic - Roughness
        //
        float metallic = material.metallicFactor;
        float roughness = material.roughnessFactor;
        if (material.metallicViewTextureIndex >= 0)
        {
            float3 matSample = materialTex[NonUniformResourceIndex(material.metallicViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0).rgb;
            metallic = matSample.b;
            roughness = matSample.g;
        }
        
        // Shadow
        float3 hitPosition = HitWorldPosition();
        Ray shadowRay = { hitPosition, normalize(-lightCB.direction) };
        bool shadowRayHit = TraceShadowRay(shadowRay, payload.recursionDepth);
        
         // Reflection (todo)
        float4 reflectedColor = float4(0, 0, 0, 0);
        
        // Final Color
        float4 phongColor = CalculatePhongLighting(baseColor, N, shadowRayHit);
        float4 color = phongColor + reflectedColor;
        
        // Visibility falloff
        float t = RayTCurrent();
        color = lerp(color, BackgroundColor, (1.0 - exp(-0.000002 * t * t * t)));
        
        payload.color = color;
    }
    
    //[shader("anyhit")]
    //void MyAnyHitShader(inout RayPayload payload, in MyAttributes attr)
    //{
    //    const MeshVertex hitSurface = GetHitSurface(attr, InstanceIndex());
        
    //    InstanceInfo instInfo = instanceData[InstanceIndex()];
    //    const MaterialData material = materialData[instInfo.MaterialIdx];

    //    // Alpha test
    //    float4 albedoSample = materialTex[NonUniformResourceIndex(material.albedoViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0);
    //    if (albedoSample.a < material.alphaCutoff)
    //        IgnoreHit();
    //}
    
    //[shader("anyhit")]
    //void MyAnyHitShader_Shadow(inout ShadowRayPayload payload, in MyAttributes attr)
    //{
    //    const MeshVertex hitSurface = GetHitSurface(attr, InstanceIndex());
        
    //    InstanceInfo instInfo = instanceData[InstanceIndex()];
    //    const MaterialData material = materialData[instInfo.MaterialIdx];

    //    // Alpha test
    //    float4 albedoSample = materialTex[NonUniformResourceIndex(material.albedoViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0);
    //    if (albedoSample.a < material.alphaCutoff)
    //        IgnoreHit();
    //}

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