#define HLSL
#include "HLSLCompatible.h"
#include "raytracingHelper.hlsli"

RaytracingAccelerationStructure scene : register(t0);
Texture2D<float> depth : register(t1);

RWTexture2D<float4> renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);
ConstantBuffer<LightData> lightCB : register(b1);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
//[numthreads(8,8,1)]
//void ShadowRayCompute(uint3 dispatchThreadID : SV_DispatchThreadID)
//    {
//        uint2 DTid = dispatchThreadID.xy;
//        if (any(DTid.xy >= sceneCB.RayDimension))
//            return;
        
//        float2 xy = DTid.xy + 0.5f;
//        float2 screenPos = xy / sceneCB.RayDimension * 2.0 - 1.0f;
        
//    // Invert y for DirectX style coord
//        screenPos.y = -screenPos.y;
        
//        float2 readGBufferAt = xy;
        
//    // Read depth and normal
//        float sceneDepth = depth.Load(int3(readGBufferAt, 0));
        
//    // Unproject into world position using depth
//        float4 unprojected = mul(sceneCB.ProjToWorld, float4(screenPos, sceneDepth, 1));
//        float3 world = unprojected.xyz / unprojected.w;
        
//    // Sun light
//        float3 direction = normalize(-lightCB.direction);
//        float3 origin = world;
        
//        RayQuery < RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > query;
//        RayDesc rayDesc;
//        rayDesc.Origin = origin;
//        rayDesc.Direction = direction;
//        rayDesc.TMin = 0.1;
//        rayDesc.TMax = 10000;
        
//        query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, rayDesc);
        
//        //while (query.Proceed())
//        //{
//        //    if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
//        //    {
//        //    // Alpha test
//        //        query.CommitNonOpaqueTriangleHit();
//        //    }
//        //    else
//        //    {
//        //    // Opaque triangle, commit hit and stop
//        //    }
//        //}
//        query.Proceed();
        
//        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
//        {
//            renderTarget[DTid] = float4(0, 0, 0, 1);
//        }
//        else
//        {
//            renderTarget[DTid] = float4(1, 1, 1, 1);
//        }
//    }
    
    [shader("raygeneration")]
    void MyRaygenShader()
    {
        uint2 DTid = DispatchRaysIndex().xy;
        float2 xy = DTid.xy + 0.5f;
        
        float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0f;
        
        // Invert y for DirectX style coord
        screenPos.y = -screenPos.y;
        
        float2 readGBufferAt = xy;
        
        // Read depth and normal
        float sceneDepth = depth.Load(int3(readGBufferAt, 0));
        
        // Unproject into world position using depth
        float4 unprojected = mul(sceneCB.ProjToWorld, float4(screenPos, sceneDepth, 1));
        float3 world = unprojected.xyz / unprojected.w;
        
    // Sun light
        float3 direction = normalize(-lightCB.direction);
        float3 origin = world;
        
        RayDesc rayDesc;
        rayDesc.Origin = origin;
        rayDesc.Direction = direction;
        rayDesc.TMin = 0.1;
        rayDesc.TMax = 10000;
        
        ShadowRayPayload payload = { true };
        TraceRay(
            scene,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            ~0, 0, 0, 0, rayDesc, payload);
        
        if (payload.hit)
        {
            renderTarget[DTid] = float4(0, 0, 0, 1);
        }
        else
        {
            renderTarget[DTid] = float4(1, 1, 1, 1);
        }
    }
    
    [shader("closesthit")]
    void MyClosestHitShader(inout ShadowRayPayload payload, in MyAttributes attr)
    {
        payload.hit = true;
    }
    [shader("miss")]
    void MyMissShader(inout ShadowRayPayload payload)
    {
        payload.hit = false;
    }