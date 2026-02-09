// SpriteVS.hlsl - Vertex shader for textured sprites

cbuffer Constants : register(b0)
{
    float4x4 WorldViewProj;
};

struct VSInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 1.0f), WorldViewProj);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}
