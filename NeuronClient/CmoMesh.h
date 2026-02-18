#pragma once

#include "GraphicsCore.h"
#include "VertexTypes.h"
#include "FileSys.h"
#include <vector>
#include <string>

namespace Neuron::Graphics
{
  /// @brief A submesh within a CMO mesh, referencing a range of indices.
  struct CmoSubmesh
  {
    uint32_t startIndex  = 0;
    uint32_t indexCount   = 0;
    uint32_t startVertex  = 0;
    uint32_t vertexCount  = 0;
    uint32_t materialIndex = 0;
  };

  /// @brief General-purpose loader for Visual Studio CMO (Compiled Mesh Object) files.
  /// Loads geometry into D3D12 vertex/index buffers using VertexPositionNormalTexture format.
  class CmoMesh
  {
  public:
    CmoMesh() = default;
    ~CmoMesh() = default;

    CmoMesh(const CmoMesh&) = delete;
    CmoMesh& operator=(const CmoMesh&) = delete;

    CmoMesh(CmoMesh&&) = default;
    CmoMesh& operator=(CmoMesh&&) = default;

    /// @brief Loads a CMO file from the assets directory.
    /// @param _filename Path relative to the Assets directory (e.g., L"Objects\\Asteroids\\asteroid_01.cmo").
    /// @return True if loading succeeded.
    bool Load(std::wstring_view _filename);

    /// @brief Releases GPU resources.
    void Destroy();

    [[nodiscard]] bool IsLoaded() const noexcept { return m_loaded; }

    [[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const noexcept { return m_vbView; }
    [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const noexcept { return m_ibView; }

    [[nodiscard]] const std::vector<CmoSubmesh>& GetSubmeshes() const noexcept { return m_submeshes; }

    [[nodiscard]] uint32_t GetTotalVertexCount() const noexcept { return m_totalVertexCount; }
    [[nodiscard]] uint32_t GetTotalIndexCount() const noexcept { return m_totalIndexCount; }

  private:
    bool ParseCmo(const byte_buffer_t& _data);
    void UploadBuffers(const std::vector<VertexPositionNormalTexture>& _vertices,
                       const std::vector<uint16_t>& _indices);

    com_ptr<ID3D12Resource> m_vertexBuffer;
    com_ptr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    D3D12_INDEX_BUFFER_VIEW  m_ibView = {};

    std::vector<CmoSubmesh> m_submeshes;
    uint32_t m_totalVertexCount = 0;
    uint32_t m_totalIndexCount  = 0;
    bool m_loaded = false;
  };
}
