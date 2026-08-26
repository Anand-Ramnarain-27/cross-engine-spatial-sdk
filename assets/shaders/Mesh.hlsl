// Unlit-ish single-directional-light shading for tile geometry. No
// textures (Material carries none yet — see docs/tile_format.md), so
// baseColor alone plus a simple Lambert term is enough to make LODs and
// materials visually distinguishable in the viewer.

// row_major + mul(matrix, vector) matches spatial::core::Mat4 exactly:
// row-major storage, column-vector convention (v' = M * v). See
// docs/architecture.md's "Core math conventions" section.
cbuffer PerDraw : register(b0)
{
    row_major float4x4 worldViewProj;
    float4 baseColor;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(worldViewProj, float4(input.position, 1.0f));
    output.normal = input.normal;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    const float3 lightDir = normalize(float3(0.4f, 0.8f, 0.4f));
    const float ndotl = saturate(dot(normalize(input.normal), lightDir));
    const float3 shaded = baseColor.rgb * (0.35f + 0.65f * ndotl);
    return float4(shaded, baseColor.a);
}
