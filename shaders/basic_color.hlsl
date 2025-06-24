cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    float4 padding[4];
};

cbuffer LightData : register(b1)
{
    float3 direction;
    float intensity;
    float3 color;
    float padded;
};

cbuffer MaterialData : register(b2)
{
    int useVertexColor;
    int useTangent;                 //  1 if tangent available, 0 use Mikktspace
    float metallicFactor;
    float roughnessFactor;
    
    int hasAlbedoMap;
    int hasMetallicRoughnessMap; // 1 if metallic roughness map available
    int hasNormalMap;               // 1 if normal map availabe
    float paddedMat;
    
    float4 baseColorFactor;
    float4x4 meshTransform; //Per-mesh transform
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

Texture2D albedoTex : register(t0);
Texture2D metallicRoughnessTex : register(t1);
Texture2D normalTex : register(t2);

SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    
    // Apply mesh transform
    float4 pos = float4(input.position, 1.f);
    pos = mul(pos, meshTransform);
    output.worldPos = pos;
    output.position = mul(pos, WorldViewProj);
    
    float4 norm = float4(input.normal, 0.f);
    output.normal = normalize(mul(norm, World).xyz);
    
    float4 tangent = float4(input.tangent.xyz, 0.f);
    output.tangent = float4(normalize(mul(tangent, World).xyz), input.tangent.w);
    
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //
    // Normal
    //
    float3x3 TBN;
    if (useTangent)
    {
        float3 T = normalize(input.tangent.xyz);
        float3 N = normalize(input.normal);
        T = normalize(T - dot(T, N) * N); /// Orthogonalize
        float3 B = normalize(cross(N, T) * input.tangent.w);
        TBN = float3x3(T, B, N);
    }
    // Compute TBN using Mikktspace (http://www.thetenthplanet.de/archives/1180)
    else
    {
        float3 N = normalize(input.normal);
        float3 dPdx = ddx(input.worldPos.xyz);
        float3 dPdy = ddy(input.worldPos.xyz);
        float2 dUVdx = ddx(input.uv);
        float2 dUVdy = ddy(input.uv);

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
    
    float3 worldNormal = normalize(input.normal);
    if (hasNormalMap)
    {
        float3 normalMap = normalTex.Sample(g_sampler, input.uv); // normal map in tangent space
        normalMap = normalMap * 2.f - 1.f;
        normalMap = normalize(normalMap);
        
        worldNormal = normalize(mul(normalMap, TBN));
    }

    //
    // Albedo
    //
    float3 albedo = baseColorFactor.rgb;
    if (useVertexColor)
    {
        albedo = input.color.rgb;
    }
    else if (hasAlbedoMap)
    {
        albedo = albedoTex.Sample(g_sampler, input.uv).rgb * baseColorFactor.rgb;
    }
    
    //
    // Metallic - Roughness
    //
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessMap)
    {
        float4 matSample = metallicRoughnessTex.Sample(g_sampler, input.uv);
        metallic = matSample.b;
        roughness = matSample.g;
    }
    //float diffuseFactor = 1.f - metallic;
    
    // Lighting calculation
    float3 lightDir = normalize(-direction);
    float NdotL = max(dot(worldNormal, lightDir), 0.0);
    
    float3 lighting = albedo * color * intensity * NdotL;
    return float4(lighting, 1.f);
}