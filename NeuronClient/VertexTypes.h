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

