cbuffer Constants : register(b0)
{
  float4x4 WorldViewProj;
  float4x4 World;
  float4   ObjectColor;
};

struct VSInput
{
  float3 position : POSITION;
  float3 normal   : NORMAL;
  float2 texcoord : TEXCOORD;
};

struct PSInput
{
  float4 position : SV_Position;
  float3 normal   : NORMAL;
  float4 color    : COLOR;
};

PSInput main(VSInput input)
{
  PSInput output;
  output.position = mul(float4(input.position, 1.0f), WorldViewProj);
  output.normal = normalize(mul(input.normal, (float3x3)World));
  output.color = ObjectColor;
  return output;
}
