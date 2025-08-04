#define HLSL
#include "HLSLCompatible.h"
#include "raytracingHelper.hlsli"

RaytracingAccelerationStructure scene : register(t0);
ByteAddressBuffer Indices : register(t1);
StructuredBuffer<MeshVertex> Vertices : register(t2);
StructuredBuffer<InstanceInfo> instanceData : register(t3);
StructuredBuffer<MaterialData> materialData : register(t4);
Texture2D<float> depth : register(t5);

Texture2D materialTex[] : register(t0, space1); // bindless for material (share desc heap)
    
RWTexture2D<float4> renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<LightData> lightCB : register(b1);

SamplerState g_sampler : register(s0);
    
    typedef BuiltInTriangleIntersectionAttributes MyAttributes;
    
    MeshVertex GetHitSurface(in float2 barycentrics, in uint instanceIdx, in uint primitiveIdx)
    {
        float3 barycentric = float3(1.f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
        
        InstanceInfo instInfo = instanceData[instanceIdx];
        
        uint primIdx = primitiveIdx; // Prim within geom
        
        uint indexByteOffset = instInfo.IdxOffsetByBytes + (primIdx * 3) * 4;
        uint idx0 = Indices.Load(indexByteOffset + 0);
        uint idx1 = Indices.Load(indexByteOffset + 4);
        uint idx2 = Indices.Load(indexByteOffset + 8);
        
        MeshVertex vtx0 = Vertices[idx0 + instInfo.VtxOffset];
        MeshVertex vtx1 = Vertices[idx1 + instInfo.VtxOffset];
        MeshVertex vtx2 = Vertices[idx2 + instInfo.VtxOffset];
        
        return BarycentricLerp(vtx0, vtx1, vtx2, barycentric);
    }
    
    [numthreads(8, 8, 1)]
    void ShadowRayCompute(uint3 dispatchThreadID : SV_DispatchThreadID)
    {
        uint2 DTid = dispatchThreadID.xy;
        if (any(DTid.xy >= sceneCB.RayDimension))
            return;
        
        float2 xy = DTid.xy + 0.5f;
        //float2 uv = xy / DispatchRaysDimensions().xy;
        float2 uv = xy / sceneCB.RayDimension;
        
        // Convert NDC [-1,1]
        float2 ndc = float2(2.f * uv.x - 1.f, 1.f - 2.f * uv.y);
        
        // Read depth and normal
        float sceneDepth = depth.Load(int3(DTid, 0));
        
        // Unproject into world position using depth
        float4 worldPos = mul(float4(ndc, sceneDepth, 1), sceneCB.ProjToWorld);
        worldPos /= worldPos.w;
        
        // Sun light
        float3 direction = normalize(-lightCB.direction);
        float3 origin = worldPos.xyz;
        
        RayQuery < RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > query;
        RayDesc rayDesc;
        rayDesc.Origin = origin;
        rayDesc.Direction = direction;
        rayDesc.TMin = 0.1;
        rayDesc.TMax = 10000;
        
        query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, rayDesc);
        
        while (query.Proceed())
        {
            if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                // Alpha test
                const MeshVertex hitSurface = GetHitSurface(
                    query.CandidateTriangleBarycentrics(),
                    query.CandidateInstanceIndex(),
                    query.CandidatePrimitiveIndex());
                InstanceInfo instInfo = instanceData[query.CandidateInstanceIndex()];
                const MaterialData material = materialData[instInfo.MaterialIdx];
                
                float4 albedoSample = materialTex[NonUniformResourceIndex(material.albedoViewTextureIndex)].SampleLevel(g_sampler, hitSurface.Uv, 0);
                // Hit opaque
                if (albedoSample.a >= material.alphaCutoff)
                    query.CommitNonOpaqueTriangleHit();
            }
        }
        
        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            renderTarget[DTid] = float4(0, 0, 0, 1);
        }
        else
        {
            renderTarget[DTid] = float4(1, 1, 1, 1);
        }
    }
    
    //[shader("raygeneration")]
    //void MyRaygenShader()
    //{
    //    uint2 DTid = DispatchRaysIndex().xy;
    //    float2 xy = DTid.xy + 0.5f;
        
    //    float2 uv = xy / DispatchRaysDimensions().xy;
        
    //    // Convert NDC [-1,1]
    //    float2 ndc = float2(2.f * uv.x - 1.f, 1.f - 2.f * uv.y);
        
    //    // Read depth and normal
    //    float sceneDepth = depth.Load(int3(DTid, 0));
        
    //    // Unproject into world position using depth
    //    float4 worldPos = mul(float4(ndc, sceneDepth, 1), sceneCB.ProjToWorld);
    //    worldPos /= worldPos.w;
    //    // Sun light
    //    float3 direction = normalize(-lightCB.direction);
    //    float3 origin = worldPos.xyz;
        
    //    RayDesc rayDesc;
    //    rayDesc.Origin = origin;
    //    rayDesc.Direction = direction;
    //    rayDesc.TMin = 0.1;
    //    rayDesc.TMax = 10000;
        
    //    ShadowRayPayload payload = { true };
    //    TraceRay(
    //        scene,
    //        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
    //        ~0, 0, 0, 0, rayDesc, payload);
        
    //    if (payload.hit)
    //    {
    //        renderTarget[DTid] = float4(0, 0, 0, 1);
    //    }
    //    else
    //    {
    //        renderTarget[DTid] = float4(1, 1, 1, 1);
    //    }
    //}
    
    //[shader("closesthit")]
    //void MyClosestHitShader(inout ShadowRayPayload payload, in MyAttributes attr)
    //{
    //    payload.hit = true;
    //}
    //[shader("miss")]
    //void MyMissShader(inout ShadowRayPayload payload)
    //{
    //    payload.hit = false;
    //}