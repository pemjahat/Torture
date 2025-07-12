struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
};

VSOutput VSMain(in uint vertexId : SV_VertexID)
{
    VSOutput output;
    
    if (vertexId == 0)
    {
        output.position = float4(-1.f, 1.f, 1.f, 1.f);
        output.texCoord = float2(0.f, 0.f);
    }
    else if (vertexId == 1)
    {
        output.position = float4(3.f, 1.f, 1.f, 1.f);
        output.texCoord = float2(2.f, 0.f);
    }
    else
    {
        output.position = float4(-1.f, -3.f, 1.f, 1.f);
        output.texCoord = float2(0.f, 2.f);
    }
    return output;
}