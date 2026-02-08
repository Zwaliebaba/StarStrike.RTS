#include "pch.h"
#include "Texture2D.h"
#include "GraphicsCore.h"
#include "GdiBitmap.h"

using namespace Neuron::Graphics;

namespace StarStrike
{
  bool Texture2D::Create(uint32_t width, uint32_t height, const uint8_t* data, DXGI_FORMAT format)
  {
    DebugTrace("Texture2D::Create - Starting creation of {}x{} texture\n", width, height);

    if (width == 0 || height == 0 || data == nullptr)
    {
      DebugTrace("Texture2D::Create - Invalid parameters: width={}, height={}, data={}\n", width, height, (data ? "valid" : "null"));
      return false;
    }

    auto device = Core::GetD3DDevice();
    if (!device)
      return false;

    m_width = width;
    m_height = height;

    // Create the texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = device->CreateCommittedResource(
      &defaultHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &texDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(m_resource.Put()));

    if (FAILED(hr))
    {
      DebugTrace("Texture2D::Create - Failed to create texture resource");
      return false;
    }

    // Calculate upload buffer size
    UINT64 uploadBufferSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

    // Create upload buffer
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    com_ptr<ID3D12Resource> uploadBuffer;
    hr = device->CreateCommittedResource(
      &uploadHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &uploadBufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadBuffer.put()));

    if (FAILED(hr))
    {
      DebugTrace("Texture2D::Create - Failed to create upload buffer");
      m_resource.Destroy();
      return false;
    }

    // Get layout for copying
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, nullptr);

    // Map and copy data to upload buffer
    void* mapped = nullptr;
    hr = uploadBuffer->Map(0, nullptr, &mapped);
    if (FAILED(hr))
    {
      DebugTrace("Texture2D::Create - Failed to map upload buffer");
      m_resource.Destroy();
      return false;
    }

    // Copy row by row (handle pitch differences)
    uint8_t* destData = static_cast<uint8_t*>(mapped) + layout.Offset;
    const uint8_t* srcData = data;
    size_t srcRowPitch = width * 4; // Assuming RGBA

    for (UINT row = 0; row < numRows; row++)
    {
      memcpy(destData + row * layout.Footprint.RowPitch, srcData + row * srcRowPitch, srcRowPitch);
    }

    uploadBuffer->Unmap(0, nullptr);

    // Copy from upload buffer to texture
    Core::ResetCommandAllocatorAndCommandlist();
    auto cmdList = Core::GetCommandList();

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer.get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = layout;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = m_resource.GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transition to shader resource state
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_resource.GetResource(),
      D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    // Update the GpuResource's tracked state
    m_resource.SetResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Execute and wait for GPU
    Core::ExecuteCommandList(true);
    Core::WaitForGpu();
    Core::ResetCommandAllocatorAndCommandlist();

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    m_srvHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(m_resource.GetResource(), &srvDesc, m_srvHandle);

    DebugTrace("Texture2D::Create - SUCCESS: Created {}x{} texture, SRV CPU ptr={}, GPU ptr={}\n", 
               width, height, m_srvHandle.GetCpuPtr(), m_srvHandle.GetGpuPtr());
    return true;
  }

  bool Texture2D::CreateFromBitmap(const Neuron::GdiBitmap* bitmap)
  {
    DebugTrace("Texture2D::CreateFromBitmap - Entry\n");

    if (!bitmap || !bitmap->pixels || bitmap->width == 0 || bitmap->height == 0)
    {
      DebugTrace("Texture2D::CreateFromBitmap - Invalid bitmap: ptr={}, pixels={}, size={}x{}\n",
                 (void*)bitmap, bitmap ? bitmap->pixels : nullptr, 
                 bitmap ? bitmap->width : 0, bitmap ? bitmap->height : 0);
      return false;
    }

    DebugTrace("Texture2D::CreateFromBitmap - Bitmap info: {}x{}, bpp={}, pitch={}, paletteSize={}\n",
               bitmap->width, bitmap->height, bitmap->bitsPerPixel, bitmap->pitch, bitmap->paletteSize);

    // Get pointer to pixel data
    const uint8_t* srcPixels = static_cast<const uint8_t*>(bitmap->pixels);

    // Convert bitmap to RGBA format
    std::vector<uint8_t> rgbaData(bitmap->width * bitmap->height * 4);

    // Use pitch for row addressing (BMP rows are DWORD-aligned)
    int srcPitch = bitmap->pitch;

    if (bitmap->bitsPerPixel == 8 && bitmap->palette)
    {
      // Indexed color with palette
      for (int y = 0; y < bitmap->height; y++)
      {
        const uint8_t* srcRow = srcPixels + y * srcPitch;
        for (int x = 0; x < bitmap->width; x++)
        {
          int dstIndex = (y * bitmap->width + x) * 4;
          uint8_t paletteIndex = srcRow[x];

          if (paletteIndex < bitmap->paletteSize)
          {
            rgbaData[dstIndex + 0] = bitmap->palette[paletteIndex].rgbRed;
            rgbaData[dstIndex + 1] = bitmap->palette[paletteIndex].rgbGreen;
            rgbaData[dstIndex + 2] = bitmap->palette[paletteIndex].rgbBlue;
            rgbaData[dstIndex + 3] = 255;
          }
          else
          {
            rgbaData[dstIndex + 0] = 0;
            rgbaData[dstIndex + 1] = 0;
            rgbaData[dstIndex + 2] = 0;
            rgbaData[dstIndex + 3] = 255;
          }
        }
      }
    }
    else if (bitmap->bitsPerPixel == 24)
    {
      // RGB format
      for (int y = 0; y < bitmap->height; y++)
      {
        const uint8_t* srcRow = srcPixels + y * srcPitch;
        for (int x = 0; x < bitmap->width; x++)
        {
          int srcX = x * 3;
          int dstIndex = (y * bitmap->width + x) * 4;

          rgbaData[dstIndex + 0] = srcRow[srcX + 2]; // B -> R
          rgbaData[dstIndex + 1] = srcRow[srcX + 1]; // G -> G
          rgbaData[dstIndex + 2] = srcRow[srcX + 0]; // R -> B
          rgbaData[dstIndex + 3] = 255;
        }
      }
    }
    else if (bitmap->bitsPerPixel == 32)
    {
      // BGRA format
      for (int y = 0; y < bitmap->height; y++)
      {
        const uint8_t* srcRow = srcPixels + y * srcPitch;
        for (int x = 0; x < bitmap->width; x++)
        {
          int srcX = x * 4;
          int dstIndex = (y * bitmap->width + x) * 4;

          rgbaData[dstIndex + 0] = srcRow[srcX + 2]; // B -> R
          rgbaData[dstIndex + 1] = srcRow[srcX + 1]; // G -> G
          rgbaData[dstIndex + 2] = srcRow[srcX + 0]; // R -> B
          rgbaData[dstIndex + 3] = srcRow[srcX + 3]; // A -> A
        }
      }
    }
    else
    {
      DebugTrace("Texture2D::CreateFromBitmap - Unsupported bits per pixel: {}", bitmap->bitsPerPixel);
      return false;
    }

    return Create(bitmap->width, bitmap->height, rgbaData.data());
  }

  bool Texture2D::CreateFromFile(const std::wstring& filename)
  {
    auto fname = FileSys::GetHomeDirectoryA() + std::string(filename.begin(), filename.end());
    DebugTrace("Texture2D::CreateFromFile - Loading: {}\n", fname);

    auto bitmap = Neuron::GdiBitmapLoader::LoadBMP(fname);

    if (!bitmap)
    {
      DebugTrace("Texture2D::CreateFromFile - FAILED to load bitmap: {}\n", fname);
      return false;
    }

    DebugTrace("Texture2D::CreateFromFile - Bitmap loaded successfully, calling CreateFromBitmap\n");
    return CreateFromBitmap(bitmap.get());
  }

  void Texture2D::Destroy()
  {
    m_resource.Destroy();
    m_width = 0;
    m_height = 0;
    m_srvHandle = {};
  }
}
