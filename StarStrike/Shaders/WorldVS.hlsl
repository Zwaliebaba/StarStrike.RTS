cbuffer Constants : register(b0)
{
  float4x4 WorldViewProj;
};

struct VSInput
{
  float3 position : POSITION;
  float4 color    : COLOR;
};

struct PSInput
{
  float4 position : SV_Position;
  float4 color    : COLOR;
};

PSInput main(VSInput input)
{
  PSInput output;
  output.position = mul(float4(input.position, 1.0f), WorldViewProj);
  output.color = input.color;
  return output;
}
