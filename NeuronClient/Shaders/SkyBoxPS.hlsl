TextureCube SkyTexture : register(t0);
SamplerState SkySampler : register(s0);

struct PSInput
{
  float4 position : SV_Position;
  float3 texcoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
  return SkyTexture.Sample(SkySampler, input.texcoord);
}
