// Ship3DPS.hlsl - Pixel shader for 3D ship rendering

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float Depth : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return input.Color;
}
