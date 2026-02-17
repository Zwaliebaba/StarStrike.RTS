Texture2D    FontTexture : register(t0);
SamplerState FontSampler : register(s0);

struct PSInput
{
  float4 position : SV_Position;
  float2 texcoord : TEXCOORD;
  float4 color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
  float4 texSample = FontTexture.Sample(FontSampler, input.texcoord);
  return texSample * input.color;
}
