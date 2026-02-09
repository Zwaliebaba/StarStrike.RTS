// BasicVS.hlsl - Simple vertex shader for position + color vertices

cbuffer Constants : register(b0)
{
    float4x4 WorldViewProj;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 1.0f), WorldViewProj);
    output.Color = input.Color;
    return output;
}
