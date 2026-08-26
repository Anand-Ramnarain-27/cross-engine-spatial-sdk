// Unlit vertex-colored line rendering for DebugRenderer's tile-bounds
// wireframes. Vertices are already in world space, so only view-projection
// is needed (no per-object world matrix).

// row_major + mul(matrix, vector) matches spatial::core::Mat4 exactly — see Mesh.hlsl.
cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(viewProj, float4(input.position, 1.0f));
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
