cbuffer Constants : register(b0)
{
  float2 InvViewportSize; // 1/width, 1/height
};

struct VSInput
{
  float3 position : POSITION;
  float2 texcoord : TEXCOORD;
  float4 color    : COLOR;
};

struct PSInput
{
  float4 position : SV_Position;
  float2 texcoord : TEXCOORD;
  float4 color    : COLOR;
};

PSInput main(VSInput input)
{
  PSInput output;

  // Transform pixel coordinates to NDC (origin top-left, Y-down)
  float x_ndc = input.position.x * InvViewportSize.x * 2.0f - 1.0f;
  float y_ndc = 1.0f - input.position.y * InvViewportSize.y * 2.0f;

  output.position = float4(x_ndc, y_ndc, 0.0f, 1.0f);
  output.texcoord = input.texcoord;
  output.color = input.color;

  return output;
}
