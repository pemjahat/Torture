// hiz.hlsl (generate hierarhical z)
/*
 Works under the assumption that it's likely that objects visible in the previous frame, will be visible this frame.

    In Phase 1, we render all objects that were visible last frame by testing against the previous HZB.
    Occluded objects are stored in a list, to be processed later.
    The HZB is constructed from the current result.
    Phase 2 tests all previously occluded objects against the new HZB and renders unoccluded.
    The HZB is constructed again from this result to be used in the next frame.
*/
Texture2D<float> InputDepth : register(t0);
RWTexture2D<float> OutputHiZ : register(u0);

cbuffer HiZConstantBuffer : register(b0)
{
    float2 OutDimensions;
    uint MipLevel;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelPos = DTid.xy;
    uint width, height, numMips;
    //OutputHiZ.GetDimensions(0, width, height, numMips);
    if (pixelPos.x >= OutDimensions.x|| pixelPos.y >= OutDimensions.y)
        return;
    
    if (MipLevel == 0)
    {
        OutputHiZ[pixelPos] = InputDepth[pixelPos];
    }
    else
    {
        uint2 srcPos = pixelPos * 2;
        float4 depths = float4(
        InputDepth[srcPos + uint2(0, 0)],
        InputDepth[srcPos + uint2(1, 0)],
        InputDepth[srcPos + uint2(0, 1)],
        InputDepth[srcPos + uint2(1, 1)]);
        OutputHiZ[pixelPos] = max(max(depths.x, depths.y), max(depths.z, depths.w));
    }
}