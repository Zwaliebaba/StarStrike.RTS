struct PSInput
{
  float4 position : SV_Position;
  float3 normal   : NORMAL;
  float4 color    : COLOR;
};

static const float3 LightDir = normalize(float3(0.3f, -1.0f, 0.5f));
static const float  Ambient  = 0.25f;

float4 main(PSInput input) : SV_Target
{
  float3 n = normalize(input.normal);
  float ndl = saturate(dot(n, -LightDir));
  float lighting = Ambient + (1.0f - Ambient) * ndl;
  return float4(input.color.rgb * lighting, input.color.a);
}
