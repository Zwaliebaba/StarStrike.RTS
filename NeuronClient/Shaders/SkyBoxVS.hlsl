cbuffer Constants : register(b0)
{
  float4x4 ViewProj; // View-projection with translation removed
};

struct VSInput
{
  float3 position : POSITION;
};

struct PSInput
{
  float4 position : SV_Position;
  float3 texcoord : TEXCOORD;
};

PSInput main(VSInput input)
{
  PSInput output;

  // Use vertex position as the cube map lookup direction
  output.texcoord = input.position;

  // Transform and force z == w so depth is always 1.0 (farthest)
  float4 pos = mul(float4(input.position, 1.0f), ViewProj);
  output.position = pos.xyww;

  return output;
}
