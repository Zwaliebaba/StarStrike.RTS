#include "pch.h"
#include "CmoMesh.h"

namespace Neuron::Graphics
{
  // CMO file format structures (Visual Studio content pipeline)
  namespace CmoFormat
  {
#pragma pack(push, 1)
    struct Vertex
    {
      XMFLOAT3 position;
      XMFLOAT3 normal;
      XMFLOAT4 tangent;
      uint32_t color;
      XMFLOAT2 texcoord;
    };
    static_assert(sizeof(Vertex) == 52, "CMO vertex size mismatch");

    struct Submesh
    {
      uint32_t materialIndex;
      uint32_t indexBufferIndex;
      uint32_t vertexBufferIndex;
      uint32_t startIndex;
      uint32_t primCount;
    };
#pragma pack(pop)
  }

  /// @brief Simple cursor for reading binary data sequentially with bounds checking.
  class BinaryReader
  {
  public:
    BinaryReader(const uint8_t* _data, size_t _size)
      : m_data(_data), m_size(_size), m_pos(0) {}

    template <typename T>
    [[nodiscard]] bool Read(T& _out)
    {
      if (m_pos + sizeof(T) > m_size)
        return false;
      std::memcpy(&_out, m_data + m_pos, sizeof(T));
      m_pos += sizeof(T);
      return true;
    }

    template <typename T>
    [[nodiscard]] bool ReadArray(T* _out, size_t _count)
    {
      size_t bytes = sizeof(T) * _count;
      if (m_pos + bytes > m_size)
        return false;
      std::memcpy(_out, m_data + m_pos, bytes);
      m_pos += bytes;
      return true;
    }

    bool Skip(size_t _bytes)
    {
      if (m_pos + _bytes > m_size)
        return false;
      m_pos += _bytes;
      return true;
    }

    bool SkipWString()
    {
      uint32_t len = 0;
      if (!Read(len))
        return false;
      return Skip(len * sizeof(wchar_t));
    }

    [[nodiscard]] size_t Remaining() const noexcept { return m_size - m_pos; }

  private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
  };

  bool CmoMesh::Load(std::wstring_view _filename)
  {
    Destroy();

    auto fileData = BinaryFile::ReadFile(_filename);
    if (fileData.empty())
    {
      DebugTrace("CmoMesh::Load failed to read file\n");
      return false;
    }

    if (!ParseCmo(fileData))
    {
      DebugTrace("CmoMesh::Load failed to parse CMO data\n");
      return false;
    }

    m_loaded = true;
    return true;
  }

  void CmoMesh::Destroy()
  {
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_vbView = {};
    m_ibView = {};
    m_submeshes.clear();
    m_totalVertexCount = 0;
    m_totalIndexCount = 0;
    m_loaded = false;
  }

  bool CmoMesh::ParseCmo(const byte_buffer_t& _data)
  {
    BinaryReader reader(_data.data(), _data.size());

    // CMO contains one or more meshes; we merge all geometry into a single VB/IB.
    std::vector<VertexPositionNormalTexture> allVertices;
    std::vector<uint16_t> allIndices;

    // Read number of meshes
    uint32_t meshCount = 0;
    if (!reader.Read(meshCount) || meshCount == 0)
      return false;

    for (uint32_t meshIdx = 0; meshIdx < meshCount; meshIdx++)
    {
      // Mesh name (wstring)
      if (!reader.SkipWString())
        return false;

      // Materials
      uint32_t materialCount = 0;
      if (!reader.Read(materialCount))
        return false;

      for (uint32_t m = 0; m < materialCount; m++)
      {
        // Material name
        if (!reader.SkipWString())
          return false;

        // Ambient, Diffuse, Specular (3x XMFLOAT4) + specular power (float) + Emissive (XMFLOAT4)
        if (!reader.Skip(sizeof(XMFLOAT4) * 4 + sizeof(float)))
          return false;

        // UV transform (XMFLOAT4X4)
        if (!reader.Skip(sizeof(XMFLOAT4X4)))
          return false;

        // Pixel shader name
        if (!reader.SkipWString())
          return false;

        // 8 texture filenames
        for (int t = 0; t < 8; t++)
        {
          if (!reader.SkipWString())
            return false;
        }
      }

      // Skeleton flag
      uint8_t hasSkeleton = 0;
      if (!reader.Read(hasSkeleton))
        return false;

      // Submeshes
      uint32_t submeshCount = 0;
      if (!reader.Read(submeshCount))
        return false;

      std::vector<CmoFormat::Submesh> rawSubmeshes(submeshCount);
      if (submeshCount > 0)
      {
        if (!reader.ReadArray(rawSubmeshes.data(), submeshCount))
          return false;
      }

      // Index buffers
      uint32_t ibCount = 0;
      if (!reader.Read(ibCount))
        return false;

      std::vector<std::vector<uint16_t>> indexBuffers(ibCount);
      for (uint32_t ib = 0; ib < ibCount; ib++)
      {
        uint32_t indexCount = 0;
        if (!reader.Read(indexCount))
          return false;

        indexBuffers[ib].resize(indexCount);
        if (indexCount > 0)
        {
          if (!reader.ReadArray(indexBuffers[ib].data(), indexCount))
            return false;
        }
      }

      // Vertex buffers
      uint32_t vbCount = 0;
      if (!reader.Read(vbCount))
        return false;

      std::vector<std::vector<CmoFormat::Vertex>> vertexBuffers(vbCount);
      for (uint32_t vb = 0; vb < vbCount; vb++)
      {
        uint32_t vertexCount = 0;
        if (!reader.Read(vertexCount))
          return false;

        vertexBuffers[vb].resize(vertexCount);
        if (vertexCount > 0)
        {
          if (!reader.ReadArray(vertexBuffers[vb].data(), vertexCount))
            return false;
        }
      }

      // Skinning data (skip if present)
      if (hasSkeleton)
      {
        // Bone count
        uint32_t boneCount = 0;
        if (!reader.Read(boneCount))
          return false;

        // Skip skinning vertex data (same count as first vertex buffer)
        if (vbCount > 0)
        {
          uint32_t skinVertCount = 0;
          if (!reader.Read(skinVertCount))
            return false;
          // Each skinning vertex: 4 bone indices (uint32) + 4 weights (float) = 32 bytes
          if (!reader.Skip(skinVertCount * 32))
            return false;
        }

        // Skip bone transforms and hierarchy
        // Inverse bind poses (boneCount * XMFLOAT4X4)
        if (!reader.Skip(boneCount * sizeof(XMFLOAT4X4)))
          return false;

        // Bone names
        for (uint32_t b = 0; b < boneCount; b++)
        {
          if (!reader.SkipWString())
            return false;
        }

        // Bone hierarchy (parent indices)
        if (!reader.Skip(boneCount * sizeof(uint32_t)))
          return false;

        // Animation clips
        uint32_t clipCount = 0;
        if (!reader.Read(clipCount))
          return false;

        for (uint32_t c = 0; c < clipCount; c++)
        {
          if (!reader.SkipWString())
            return false;
          float startTime = 0, endTime = 0;
          if (!reader.Read(startTime) || !reader.Read(endTime))
            return false;

          uint32_t keyCount = 0;
          if (!reader.Read(keyCount))
            return false;

          // Each key: boneIndex(uint32) + time(float) + translation(float3) + rotation(float4) + scale(float3)
          if (!reader.Skip(keyCount * (sizeof(uint32_t) + sizeof(float) + sizeof(XMFLOAT3) + sizeof(XMFLOAT4) + sizeof(XMFLOAT3))))
            return false;
        }
      }

      // Convert and merge into combined buffers
      uint32_t baseVertex = static_cast<uint32_t>(allVertices.size());

      for (uint32_t s = 0; s < submeshCount; s++)
      {
        const auto& sub = rawSubmeshes[s];
        if (sub.vertexBufferIndex >= vbCount || sub.indexBufferIndex >= ibCount)
          continue;

        const auto& srcVerts = vertexBuffers[sub.vertexBufferIndex];
        const auto& srcIndices = indexBuffers[sub.indexBufferIndex];

        // Convert all CMO vertices from this VB to VertexPositionNormalTexture
        uint32_t vertOffset = static_cast<uint32_t>(allVertices.size());
        for (size_t v = 0; v < srcVerts.size(); v++)
        {
          const auto& sv = srcVerts[v];
          allVertices.push_back({sv.position, sv.normal, sv.texcoord});
        }

        // Index count = primCount * 3 (triangles)
        uint32_t indexCount = sub.primCount * 3;

        // Reverse winding from CMO clockwise to counter-clockwise
        uint32_t idxStart = static_cast<uint32_t>(allIndices.size());
        for (uint32_t i = sub.startIndex; i + 2 < sub.startIndex + indexCount && i + 2 < srcIndices.size(); i += 3)
        {
          allIndices.push_back(static_cast<uint16_t>(srcIndices[i]     + (vertOffset - baseVertex)));
          allIndices.push_back(static_cast<uint16_t>(srcIndices[i + 2] + (vertOffset - baseVertex)));
          allIndices.push_back(static_cast<uint16_t>(srcIndices[i + 1] + (vertOffset - baseVertex)));
        }

        CmoSubmesh outSub;
        outSub.startVertex = vertOffset;
        outSub.vertexCount = static_cast<uint32_t>(srcVerts.size());
        outSub.startIndex = idxStart;
        outSub.indexCount = static_cast<uint32_t>(allIndices.size()) - idxStart;
        outSub.materialIndex = sub.materialIndex;
        m_submeshes.push_back(outSub);
      }
    }

    if (allVertices.empty() || allIndices.empty())
      return false;

    m_totalVertexCount = static_cast<uint32_t>(allVertices.size());
    m_totalIndexCount = static_cast<uint32_t>(allIndices.size());

    UploadBuffers(allVertices, allIndices);
    return true;
  }

  void CmoMesh::UploadBuffers(const std::vector<VertexPositionNormalTexture>& _vertices,
                               const std::vector<uint16_t>& _indices)
  {
    auto* device = Core::GetD3DDevice();

    const UINT vbSize = static_cast<UINT>(_vertices.size() * sizeof(VertexPositionNormalTexture));
    const UINT ibSize = static_cast<UINT>(_indices.size() * sizeof(uint16_t));

    // Create default heap resources
    auto defaultProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

    check_hresult(device->CreateCommittedResource(
      &defaultProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_vertexBuffer.put())));

    check_hresult(device->CreateCommittedResource(
      &defaultProps, D3D12_HEAP_FLAG_NONE, &ibDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_indexBuffer.put())));

    // Create upload heaps
    auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    com_ptr<ID3D12Resource> vbUpload;
    com_ptr<ID3D12Resource> ibUpload;

    check_hresult(device->CreateCommittedResource(
      &uploadProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(vbUpload.put())));

    check_hresult(device->CreateCommittedResource(
      &uploadProps, D3D12_HEAP_FLAG_NONE, &ibDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ibUpload.put())));

    // Map, copy, unmap
    void* mapped = nullptr;
    check_hresult(vbUpload->Map(0, nullptr, &mapped));
    memcpy(mapped, _vertices.data(), vbSize);
    vbUpload->Unmap(0, nullptr);

    check_hresult(ibUpload->Map(0, nullptr, &mapped));
    memcpy(mapped, _indices.data(), ibSize);
    ibUpload->Unmap(0, nullptr);

    // Record copy commands
    auto* cmdList = Core::GetCommandList();
    bool wasOpen = Core::IsCommandListOpen();
    if (!wasOpen)
      Core::ResetCommandAllocatorAndCommandlist();

    auto vbBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    auto ibBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_indexBuffer.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_RESOURCE_BARRIER barriers[] = {vbBarrier, ibBarrier};
    cmdList->ResourceBarrier(2, barriers);

    cmdList->CopyResource(m_vertexBuffer.get(), vbUpload.get());
    cmdList->CopyResource(m_indexBuffer.get(), ibUpload.get());

    auto vbBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto ibBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_indexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    D3D12_RESOURCE_BARRIER barriers2[] = {vbBarrier2, ibBarrier2};
    cmdList->ResourceBarrier(2, barriers2);

    Core::ExecuteCommandList(true);
    Core::WaitForGpu();

    if (wasOpen)
      Core::ResetCommandAllocatorAndCommandlist();

    // Set up views
    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = vbSize;
    m_vbView.StrideInBytes = sizeof(VertexPositionNormalTexture);

    m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_ibView.SizeInBytes = ibSize;
    m_ibView.Format = DXGI_FORMAT_R16_UINT;
  }
}
