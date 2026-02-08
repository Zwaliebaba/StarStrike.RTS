#pragma once

struct VertexPosition
{
  VertexPosition() = default;

  VertexPosition(const VertexPosition&) = default;
  VertexPosition& operator=(const VertexPosition&) = default;

  VertexPosition(VertexPosition&&) = default;
  VertexPosition& operator=(VertexPosition&&) = default;

  VertexPosition(const XMFLOAT3& iposition) noexcept
    : m_position(iposition) {}

  VertexPosition(FXMVECTOR iposition) noexcept { XMStoreFloat3(&this->m_position, iposition); }

  XMFLOAT3 m_position;

  static constexpr unsigned int INPUT_ELEMENT_COUNT = 1;

  static constexpr D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  static constexpr D3D12_INPUT_LAYOUT_DESC INPUT_LAYOUT{INPUT_ELEMENTS, INPUT_ELEMENT_COUNT};
};

static_assert(sizeof(VertexPosition) == 12, "Vertex struct/layout mismatch");

struct VertexPositionColor
{
  VertexPositionColor() = default;

  VertexPositionColor(const VertexPositionColor&) = default;
  VertexPositionColor& operator=(const VertexPositionColor&) = default;

  VertexPositionColor(VertexPositionColor&&) = default;
  VertexPositionColor& operator=(VertexPositionColor&&) = default;

  VertexPositionColor(const XMFLOAT3& iposition, const XMFLOAT4& icolor) noexcept
    : m_position(iposition), m_color(icolor) {}

  VertexPositionColor(FXMVECTOR iposition, FXMVECTOR icolor) noexcept
  {
    XMStoreFloat3(&m_position, iposition);
    XMStoreFloat4(&m_color, icolor);
  }

  XMFLOAT3 m_position;
  XMFLOAT4 m_color;

  static constexpr unsigned int INPUT_ELEMENT_COUNT = 2;

  static constexpr D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  static constexpr D3D12_INPUT_LAYOUT_DESC INPUT_LAYOUT{INPUT_ELEMENTS, INPUT_ELEMENT_COUNT};
};

static_assert(sizeof(VertexPositionColor) == 28, "Vertex struct/layout mismatch");

struct VertexPositionTexture
{
  VertexPositionTexture() = default;

  VertexPositionTexture(const VertexPositionTexture&) = default;
  VertexPositionTexture& operator=(const VertexPositionTexture&) = default;

  VertexPositionTexture(VertexPositionTexture&&) = default;
  VertexPositionTexture& operator=(VertexPositionTexture&&) = default;

  VertexPositionTexture(const XMFLOAT3& iposition, const XMFLOAT2& itexcoord) noexcept
    : m_position(iposition), m_texcoord(itexcoord) {}

  XMFLOAT3 m_position;
  XMFLOAT2 m_texcoord;

  static constexpr unsigned int INPUT_ELEMENT_COUNT = 2;

  static constexpr D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  static constexpr D3D12_INPUT_LAYOUT_DESC INPUT_LAYOUT{INPUT_ELEMENTS, INPUT_ELEMENT_COUNT};
};

static_assert(sizeof(VertexPositionTexture) == 20, "Vertex struct/layout mismatch");

struct VertexPositionTextureColor
{
  VertexPositionTextureColor() = default;

  VertexPositionTextureColor(const VertexPositionTextureColor&) = default;
  VertexPositionTextureColor& operator=(const VertexPositionTextureColor&) = default;

  VertexPositionTextureColor(VertexPositionTextureColor&&) = default;
  VertexPositionTextureColor& operator=(VertexPositionTextureColor&&) = default;

  VertexPositionTextureColor(const XMFLOAT3& iposition, const XMFLOAT2& itexcoord, const XMFLOAT4& icolor) noexcept
    : m_position(iposition), m_texcoord(itexcoord), m_color(icolor) {}

  XMFLOAT3 m_position;
  XMFLOAT2 m_texcoord;
  XMFLOAT4 m_color;

  static constexpr unsigned int INPUT_ELEMENT_COUNT = 3;

  static constexpr D3D12_INPUT_ELEMENT_DESC INPUT_ELEMENTS[INPUT_ELEMENT_COUNT]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  static constexpr D3D12_INPUT_LAYOUT_DESC INPUT_LAYOUT{INPUT_ELEMENTS, INPUT_ELEMENT_COUNT};
};

static_assert(sizeof(VertexPositionTextureColor) == 36, "Vertex struct/layout mismatch");

