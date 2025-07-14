struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct RaygenConstantBuffer
{
    Viewport viewport;
    Viewport stencil;
};

RaytracingAccelerationStructure scene : register(t0);
RWTexture2D<float4> renderTarget : register(u0);
ConstantBuffer<RaygenConstantBuffer> raygenCB : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom)
}

[shader["raygeneration"]]
void MyRaygenShader()
{
    float2 lerpValues = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions();
    
    // Orthographics proj since we're tracing in screen space
    float3 rayDir = float3(0, 0, 1);
    float3 origin = float3(
        lerp(raygenCB.viewport.left, raygenCB.viewport.right, lerpValues.x),
        lerp(raygenCB.viewport.top, raygenCB.viewport.bottom, lerpValues.y),
        0.f);
    
    if (IsInsideViewport(origin.xy, raygenCB.stencil))
    {
        // Trace
        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = rayDir;
        // set TMin to non-zero small value to avoid aliasing issue (float point)
        ray.TMin = 0.001f;
        ray.TMax = 10000.f;
        RayPayload payload = { float4(0, 0, 0, 0) };
        TraceRay(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
        
        // Write into output
        renderTarget[DispatchRaysIndex().xy] = payload.color;
    }
    else
    {
        renderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
    }
}

[shader["closesthit"]]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 barrycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(barrycentrics, 1.f);
}

[shader["miss"]]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 0);
}