// Ship3DVS.hlsl - Vertex shader for 3D ship rendering

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
    float Depth : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 1.0f), WorldViewProj);
    output.Color = input.Color;
    output.Depth = output.Position.z / output.Position.w;
    return output;
}
