#include "pch.h"
#include "DDSTextureLoader.h"
#include "FileSys.h"

using namespace Neuron;

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//--------------------------------------------------------------------------------------

namespace
{
  constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

  // DDS pixel format flags
  constexpr uint32_t DDS_FOURCC    = 0x00000004; // DDPF_FOURCC
  constexpr uint32_t DDS_RGB       = 0x00000040; // DDPF_RGB
  constexpr uint32_t DDS_LUMINANCE = 0x00020000; // DDPF_LUMINANCE
  constexpr uint32_t DDS_ALPHA     = 0x00000002; // DDPF_ALPHA
  constexpr uint32_t DDS_BUMPDUDV  = 0x00080000; // DDPF_BUMPDUDV

  // DDS header flags
  constexpr uint32_t DDS_HEADER_FLAGS_VOLUME = 0x00800000; // DDSD_DEPTH

  // DDS cubemap flags
  constexpr uint32_t DDS_CUBEMAP_POSITIVEX = 0x00000600;
  constexpr uint32_t DDS_CUBEMAP_NEGATIVEX = 0x00000a00;
  constexpr uint32_t DDS_CUBEMAP_POSITIVEY = 0x00001200;
  constexpr uint32_t DDS_CUBEMAP_NEGATIVEY = 0x00002200;
  constexpr uint32_t DDS_CUBEMAP_POSITIVEZ = 0x00004200;
  constexpr uint32_t DDS_CUBEMAP_NEGATIVEZ = 0x00008200;
  constexpr uint32_t DDS_CUBEMAP_ALLFACES  = DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |
                                             DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |
                                             DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ;
  constexpr uint32_t DDS_CUBEMAP = 0x00000200;

  struct DDS_PIXELFORMAT
  {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
  };

  struct DDS_HEADER
  {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
  };

  struct DDS_HEADER_DXT10
  {
    DXGI_FORMAT dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
  };

  constexpr uint32_t MAKE_FOUR_CC(char _ch0, char _ch1, char _ch2, char _ch3) noexcept
  {
    return static_cast<uint32_t>(static_cast<uint8_t>(_ch0)) |
           (static_cast<uint32_t>(static_cast<uint8_t>(_ch1)) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(_ch2)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(_ch3)) << 24);
  }

  //--------------------------------------------------------------------------------------
  // Return the BPP for a particular format
  //--------------------------------------------------------------------------------------
  size_t BitsPerPixel(DXGI_FORMAT fmt) noexcept
  {
    switch (fmt)
    {
      case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_UINT:
      case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

      case DXGI_FORMAT_R32G32B32_TYPELESS:
      case DXGI_FORMAT_R32G32B32_FLOAT:
      case DXGI_FORMAT_R32G32B32_UINT:
      case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R16G16B16A16_SNORM:
      case DXGI_FORMAT_R16G16B16A16_SINT:
      case DXGI_FORMAT_R32G32_TYPELESS:
      case DXGI_FORMAT_R32G32_FLOAT:
      case DXGI_FORMAT_R32G32_UINT:
      case DXGI_FORMAT_R32G32_SINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      case DXGI_FORMAT_Y416:
      case DXGI_FORMAT_Y210:
      case DXGI_FORMAT_Y216:
        return 64;

      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R11G11B10_FLOAT:
      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R8G8B8A8_SNORM:
      case DXGI_FORMAT_R8G8B8A8_SINT:
      case DXGI_FORMAT_R16G16_TYPELESS:
      case DXGI_FORMAT_R16G16_FLOAT:
      case DXGI_FORMAT_R16G16_UNORM:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R16G16_SNORM:
      case DXGI_FORMAT_R16G16_SINT:
      case DXGI_FORMAT_R32_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_UINT:
      case DXGI_FORMAT_R32_SINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
      case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      case DXGI_FORMAT_R8G8_B8G8_UNORM:
      case DXGI_FORMAT_G8R8_G8B8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
      case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8X8_TYPELESS:
      case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      case DXGI_FORMAT_AYUV:
      case DXGI_FORMAT_Y410:
      case DXGI_FORMAT_YUY2:
        return 32;

      case DXGI_FORMAT_P010:
      case DXGI_FORMAT_P016:
        return 24;

      case DXGI_FORMAT_R8G8_TYPELESS:
      case DXGI_FORMAT_R8G8_UNORM:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R8G8_SNORM:
      case DXGI_FORMAT_R8G8_SINT:
      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R16_SNORM:
      case DXGI_FORMAT_R16_SINT:
      case DXGI_FORMAT_B5G6R5_UNORM:
      case DXGI_FORMAT_B5G5R5A1_UNORM:
      case DXGI_FORMAT_A8P8:
      case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_420_OPAQUE:
      case DXGI_FORMAT_NV11:
        return 12;

      case DXGI_FORMAT_R8_TYPELESS:
      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_R8_UINT:
      case DXGI_FORMAT_R8_SNORM:
      case DXGI_FORMAT_R8_SINT:
      case DXGI_FORMAT_A8_UNORM:
      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB:
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB:
      case DXGI_FORMAT_BC5_TYPELESS:
      case DXGI_FORMAT_BC5_UNORM:
      case DXGI_FORMAT_BC5_SNORM:
      case DXGI_FORMAT_BC6H_TYPELESS:
      case DXGI_FORMAT_BC6H_UF16:
      case DXGI_FORMAT_BC6H_SF16:
      case DXGI_FORMAT_BC7_TYPELESS:
      case DXGI_FORMAT_BC7_UNORM:
      case DXGI_FORMAT_BC7_UNORM_SRGB:
      case DXGI_FORMAT_AI44:
      case DXGI_FORMAT_IA44:
      case DXGI_FORMAT_P8:
        return 8;

      case DXGI_FORMAT_R1_UNORM:
        return 1;

      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB:
      case DXGI_FORMAT_BC4_TYPELESS:
      case DXGI_FORMAT_BC4_UNORM:
      case DXGI_FORMAT_BC4_SNORM:
        return 4;

      default:
        return 0;
    }
  }

  //--------------------------------------------------------------------------------------
  // Get surface information for a particular format
  //--------------------------------------------------------------------------------------
  HRESULT GetSurfaceInfo(size_t width, size_t height, DXGI_FORMAT fmt, size_t *outNumBytes, size_t *outRowBytes, size_t *outNumRows) noexcept
  {
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    bool bc = false;
    bool packed = false;
    bool planar = false;
    size_t bpe = 0;
    switch (fmt)
    {
      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB:
      case DXGI_FORMAT_BC4_TYPELESS:
      case DXGI_FORMAT_BC4_UNORM:
      case DXGI_FORMAT_BC4_SNORM:
        bc = true;
        bpe = 8;
        break;

      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB:
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB:
      case DXGI_FORMAT_BC5_TYPELESS:
      case DXGI_FORMAT_BC5_UNORM:
      case DXGI_FORMAT_BC5_SNORM:
      case DXGI_FORMAT_BC6H_TYPELESS:
      case DXGI_FORMAT_BC6H_UF16:
      case DXGI_FORMAT_BC6H_SF16:
      case DXGI_FORMAT_BC7_TYPELESS:
      case DXGI_FORMAT_BC7_UNORM:
      case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc = true;
        bpe = 16;
        break;

      case DXGI_FORMAT_R8G8_B8G8_UNORM:
      case DXGI_FORMAT_G8R8_G8B8_UNORM:
      case DXGI_FORMAT_YUY2:
        packed = true;
        bpe = 4;
        break;

      case DXGI_FORMAT_Y210:
      case DXGI_FORMAT_Y216:
        packed = true;
        bpe = 8;
        break;

      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_420_OPAQUE:
        planar = true;
        bpe = 2;
        break;

      case DXGI_FORMAT_P010:
      case DXGI_FORMAT_P016:
        planar = true;
        bpe = 4;
        break;

      default:
        break;
    }

    if (bc)
    {
      uint64_t numBlocksWide = 0;
      if (width > 0) numBlocksWide = std::max<uint64_t>(1u, (static_cast<uint64_t>(width) + 3u) / 4u);
      uint64_t numBlocksHigh = 0;
      if (height > 0) numBlocksHigh = std::max<uint64_t>(1u, (static_cast<uint64_t>(height) + 3u) / 4u);
      rowBytes = numBlocksWide * bpe;
      numRows = numBlocksHigh;
      numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed)
    {
      rowBytes = ((static_cast<uint64_t>(width) + 1u) >> 1) * bpe;
      numRows = static_cast<uint64_t>(height);
      numBytes = rowBytes * height;
    }
    else if (fmt == DXGI_FORMAT_NV11)
    {
      rowBytes = ((static_cast<uint64_t>(width) + 3u) >> 2) * 4u;
      numRows = static_cast<uint64_t>(height) * 2u;
      numBytes = rowBytes * numRows;
    }
    else if (planar)
    {
      rowBytes = ((static_cast<uint64_t>(width) + 1u) >> 1) * bpe;
      numBytes = (rowBytes * static_cast<uint64_t>(height)) + ((rowBytes * static_cast<uint64_t>(height) + 1u) >> 1);
      numRows = height + ((static_cast<uint64_t>(height) + 1u) >> 1);
    }
    else
    {
      size_t bpp = BitsPerPixel(fmt);
      if (!bpp) return E_INVALIDARG;

      rowBytes = (static_cast<uint64_t>(width) * bpp + 7u) / 8u;
      numRows = static_cast<uint64_t>(height);
      numBytes = rowBytes * height;
    }

    if (outNumBytes) *outNumBytes = numBytes;
    if (outRowBytes) *outRowBytes = rowBytes;
    if (outNumRows) *outNumRows = numRows;

    return S_OK;
  }

  //--------------------------------------------------------------------------------------
  DXGI_FORMAT GetDXGIFormat(const DDS_PIXELFORMAT &ddpf) noexcept
  {
    if (ddpf.flags & DDS_RGB)
    {
      switch (ddpf.RGBBitCount)
      {
        case 32:
          if (ddpf.RBitMask == 0x000000ff && ddpf.GBitMask == 0x0000ff00 && ddpf.BBitMask == 0x00ff0000 && ddpf.ABitMask == 0xff000000) return DXGI_FORMAT_R8G8B8A8_UNORM;

          if (ddpf.RBitMask == 0x00ff0000 && ddpf.GBitMask == 0x0000ff00 && ddpf.BBitMask == 0x000000ff && ddpf.ABitMask == 0xff000000) return DXGI_FORMAT_B8G8R8A8_UNORM;

          if (ddpf.RBitMask == 0x00ff0000 && ddpf.GBitMask == 0x0000ff00 && ddpf.BBitMask == 0x000000ff && ddpf.ABitMask == 0x00000000) return DXGI_FORMAT_B8G8R8X8_UNORM;

          if (ddpf.RBitMask == 0x0000ffff && ddpf.GBitMask == 0xffff0000 && ddpf.BBitMask == 0x00000000 && ddpf.ABitMask == 0x00000000) return DXGI_FORMAT_R16G16_UNORM;

          if (ddpf.RBitMask == 0xffffffff && ddpf.GBitMask == 0x00000000 && ddpf.BBitMask == 0x00000000 && ddpf.ABitMask == 0x00000000) return DXGI_FORMAT_R32_FLOAT;
          break;

        case 24:
          break;

        case 16:
          if (ddpf.RBitMask == 0x7c00 && ddpf.GBitMask == 0x03e0 && ddpf.BBitMask == 0x001f && ddpf.ABitMask == 0x8000) return DXGI_FORMAT_B5G5R5A1_UNORM;
          if (ddpf.RBitMask == 0xf800 && ddpf.GBitMask == 0x07e0 && ddpf.BBitMask == 0x001f && ddpf.ABitMask == 0x0000) return DXGI_FORMAT_B5G6R5_UNORM;
          if (ddpf.RBitMask == 0x0f00 && ddpf.GBitMask == 0x00f0 && ddpf.BBitMask == 0x000f && ddpf.ABitMask == 0xf000) return DXGI_FORMAT_B4G4R4A4_UNORM;
          break;
      }
    }
    else if (ddpf.flags & DDS_LUMINANCE)
    {
      if (8 == ddpf.RGBBitCount) { if (ddpf.RBitMask == 0xff && ddpf.GBitMask == 0x00 && ddpf.BBitMask == 0x00 && ddpf.ABitMask == 0x00) return DXGI_FORMAT_R8_UNORM; }

      if (16 == ddpf.RGBBitCount)
      {
        if (ddpf.RBitMask == 0x00ff && ddpf.GBitMask == 0x00 && ddpf.BBitMask == 0x00 && ddpf.ABitMask == 0xff00) return DXGI_FORMAT_R8G8_UNORM;
        if (ddpf.RBitMask == 0xffff && ddpf.GBitMask == 0x0000 && ddpf.BBitMask == 0x0000 && ddpf.ABitMask == 0x0000) return DXGI_FORMAT_R16_UNORM;
      }
    }
    else if (ddpf.flags & DDS_ALPHA) { if (8 == ddpf.RGBBitCount) return DXGI_FORMAT_A8_UNORM; }
    else if (ddpf.flags & DDS_BUMPDUDV)
    {
      if (16 == ddpf.RGBBitCount) { if (ddpf.RBitMask == 0x00ff && ddpf.GBitMask == 0xff00 && ddpf.BBitMask == 0x0000 && ddpf.ABitMask == 0x0000) return DXGI_FORMAT_R8G8_SNORM; }

      if (32 == ddpf.RGBBitCount)
      {
        if (ddpf.RBitMask == 0x000000ff && ddpf.GBitMask == 0x0000ff00 && ddpf.BBitMask == 0x00ff0000 && ddpf.ABitMask == 0xff000000) return DXGI_FORMAT_R8G8B8A8_SNORM;
        if (ddpf.RBitMask == 0x0000ffff && ddpf.GBitMask == 0xffff0000 && ddpf.BBitMask == 0x00000000 && ddpf.ABitMask == 0x00000000) return DXGI_FORMAT_R16G16_SNORM;
      }
    }
    else if (ddpf.flags & DDS_FOURCC)
    {
      if (MAKE_FOUR_CC('D', 'X', 'T', '1') == ddpf.fourCC) return DXGI_FORMAT_BC1_UNORM;
      if (MAKE_FOUR_CC('D', 'X', 'T', '3') == ddpf.fourCC) return DXGI_FORMAT_BC2_UNORM;
      if (MAKE_FOUR_CC('D', 'X', 'T', '5') == ddpf.fourCC) return DXGI_FORMAT_BC3_UNORM;
      if (MAKE_FOUR_CC('D', 'X', 'T', '2') == ddpf.fourCC) return DXGI_FORMAT_BC2_UNORM;
      if (MAKE_FOUR_CC('D', 'X', 'T', '4') == ddpf.fourCC) return DXGI_FORMAT_BC3_UNORM;

      if (MAKE_FOUR_CC('A', 'T', 'I', '1') == ddpf.fourCC) return DXGI_FORMAT_BC4_UNORM;
      if (MAKE_FOUR_CC('B', 'C', '4', 'U') == ddpf.fourCC) return DXGI_FORMAT_BC4_UNORM;
      if (MAKE_FOUR_CC('B', 'C', '4', 'S') == ddpf.fourCC) return DXGI_FORMAT_BC4_SNORM;

      if (MAKE_FOUR_CC('A', 'T', 'I', '2') == ddpf.fourCC) return DXGI_FORMAT_BC5_UNORM;
      if (MAKE_FOUR_CC('B', 'C', '5', 'U') == ddpf.fourCC) return DXGI_FORMAT_BC5_UNORM;
      if (MAKE_FOUR_CC('B', 'C', '5', 'S') == ddpf.fourCC) return DXGI_FORMAT_BC5_SNORM;

      if (MAKE_FOUR_CC('R', 'G', 'B', 'G') == ddpf.fourCC) return DXGI_FORMAT_R8G8_B8G8_UNORM;
      if (MAKE_FOUR_CC('G', 'R', 'G', 'B') == ddpf.fourCC) return DXGI_FORMAT_G8R8_G8B8_UNORM;

      if (MAKE_FOUR_CC('Y', 'U', 'Y', '2') == ddpf.fourCC) return DXGI_FORMAT_YUY2;

      switch (ddpf.fourCC)
      {
        case 36:
          return DXGI_FORMAT_R16G16B16A16_UNORM;
        case 110:
          return DXGI_FORMAT_R16G16B16A16_SNORM;
        case 111:
          return DXGI_FORMAT_R16_FLOAT;
        case 112:
          return DXGI_FORMAT_R16G16_FLOAT;
        case 113:
          return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case 114:
          return DXGI_FORMAT_R32_FLOAT;
        case 115:
          return DXGI_FORMAT_R32G32_FLOAT;
        case 116:
          return DXGI_FORMAT_R32G32B32A32_FLOAT;
      }
    }



    return DXGI_FORMAT_UNKNOWN;
  }

  //--------------------------------------------------------------------------------------
  HRESULT FillInitData(size_t width, size_t height, size_t depth, size_t mipCount, size_t arraySize, DXGI_FORMAT format, size_t maxsize, size_t bitSize, const uint8_t *bitData, size_t &twidth, size_t &theight, size_t &tdepth, size_t &skipMip, std::vector<D3D12_SUBRESOURCE_DATA> &subresources)
  {
    if (!bitData) return E_POINTER;

    skipMip = 0;
    twidth = 0;
    theight = 0;
    tdepth = 0;

    size_t NumBytes = 0;
    size_t RowBytes = 0;
    const uint8_t *pSrcBits = bitData;
    const uint8_t *pEndBits = bitData + bitSize;

    subresources.clear();

    for (size_t j = 0; j < arraySize; j++)
    {
      size_t w = width;
      size_t h = height;
      size_t d = depth;
      for (size_t i = 0; i < mipCount; i++)
      {
        HRESULT hr = GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);
        if (FAILED(hr)) return hr;

        if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX) return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

        if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
        {
          if (!twidth)
          {
            twidth = w;
            theight = h;
            tdepth = d;
          }

          D3D12_SUBRESOURCE_DATA res = {};
          res.pData = pSrcBits;
          res.RowPitch = static_cast<LONG_PTR>(RowBytes);
          res.SlicePitch = static_cast<LONG_PTR>(NumBytes);
          subresources.push_back(res);
        }
        else if (!j) skipMip++;

        if (pSrcBits + (NumBytes * d) > pEndBits) return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        pSrcBits += NumBytes * d;

        w = w >> 1;
        h = h >> 1;
        d = d >> 1;
        if (w == 0) w = 1;
        if (h == 0) h = 1;
        if (d == 0) d = 1;
      }
    }

    return subresources.empty() ? E_FAIL : S_OK;
  }

  //--------------------------------------------------------------------------------------
  HRESULT CreateTextureResource(ID3D12Device *d3dDevice, D3D12_RESOURCE_DIMENSION resDim, size_t width, size_t height, size_t depth, size_t mipCount, size_t arraySize, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resFlags, ID3D12Resource **texture) noexcept
  {
    if (!d3dDevice) return E_POINTER;

    HRESULT hr = E_FAIL;

    D3D12_RESOURCE_DESC desc = {};
    desc.Width = width;
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = static_cast<UINT16>(mipCount);
    desc.DepthOrArraySize = (resDim == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? static_cast<UINT16>(depth) : static_cast<UINT16>(arraySize);
    desc.Format = format;
    desc.Flags = resFlags;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Dimension = resDim;

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    hr = d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(texture));

    return hr;
  }

  //--------------------------------------------------------------------------------------
  HRESULT CreateTextureFromDDS(ID3D12Device *d3dDevice, const DDS_HEADER *header, const uint8_t *bitData, size_t bitSize, size_t maxsize, D3D12_RESOURCE_FLAGS resFlags, ID3D12Resource **texture, std::vector<D3D12_SUBRESOURCE_DATA> &subresources, bool *outIsCubeMap) noexcept
  {
    HRESULT hr = S_OK;

    const size_t width = header->width;
    size_t height = header->height;
    size_t depth = header->depth;

    D3D12_RESOURCE_DIMENSION resDim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
    size_t arraySize = 1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isCubeMap = false;

    size_t mipCount = header->mipMapCount;
    if (0 == mipCount) mipCount = 1;

    if ((header->ddspf.flags & DDS_FOURCC) && (MAKE_FOUR_CC('D', 'X', '1', '0') == header->ddspf.fourCC))
    {
      const auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10 *>(reinterpret_cast<const uint8_t *>(header) + sizeof(DDS_HEADER));

      arraySize = d3d10ext->arraySize;
      if (arraySize == 0) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

      switch (d3d10ext->dxgiFormat)
      {
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
          return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        default:
          if (BitsPerPixel(d3d10ext->dxgiFormat) == 0) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
      }

      format = d3d10ext->dxgiFormat;

      switch (d3d10ext->resourceDimension)
      {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
          if ((header->flags & DDS_HEADER_FLAGS_VOLUME) || (height != 1)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
          height = depth = 1;
          break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
          if (d3d10ext->miscFlag & 0x4)// RESOURCE_MISC_TEXTURECUBE
          {
            arraySize *= 6;
            isCubeMap = true;
          }
          depth = 1;
          break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
          if (!(header->flags & DDS_HEADER_FLAGS_VOLUME)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
          if (arraySize > 1) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
          break;

        default:
          return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
      }

      resDim = static_cast<D3D12_RESOURCE_DIMENSION>(d3d10ext->resourceDimension);
    }
    else
    {
      format = GetDXGIFormat(header->ddspf);

      if (format == DXGI_FORMAT_UNKNOWN) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

      if (header->flags & DDS_HEADER_FLAGS_VOLUME) resDim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
      else
      {
        if (header->caps2 & DDS_CUBEMAP)
        {
          if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

          arraySize = 6;
          isCubeMap = true;
        }

        depth = 1;
        resDim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      }

      DEBUG_ASSERT(BitsPerPixel(format) != 0);
    }

    if (outIsCubeMap) *outIsCubeMap = isCubeMap;

    // Create the texture
    size_t numberOfPlanes = 1;

    if (arraySize > UINT16_MAX || mipCount > UINT16_MAX) return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    if ((maxsize > 0) && ((width > maxsize) || (height > maxsize) || (depth > maxsize))) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    size_t twidth = 0;
    size_t theight = 0;
    size_t tdepth = 0;
    size_t skipMip = 0;

    hr = FillInitData(width, height, depth, mipCount, arraySize * numberOfPlanes, format, maxsize, bitSize, bitData, twidth, theight, tdepth, skipMip, subresources);
    if (FAILED(hr)) return hr;

    hr = CreateTextureResource(d3dDevice, resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize, format, resFlags, texture);
    if (FAILED(hr)) return hr;

    return hr;
  }
}// anonymous namespace

//--------------------------------------------------------------------------------------
// Public entry points
//--------------------------------------------------------------------------------------

HRESULT Neuron::Graphics::LoadDDSTextureFromMemory(ID3D12Device *_d3dDevice, const uint8_t *_ddsData, size_t _ddsDataSize, ID3D12Resource **_texture, std::vector<D3D12_SUBRESOURCE_DATA> &_subresources, size_t _maxSize, DDS_ALPHA_MODE *_alphaMode, bool *_isCubeMap)
{
  if (_texture) *_texture = nullptr;
  if (_alphaMode) *_alphaMode = DDS_ALPHA_MODE_UNKNOWN;
  if (_isCubeMap) *_isCubeMap = false;

  if (!_d3dDevice || !_ddsData || !_texture)
  {
    DebugTrace("LoadDDSTextureFromMemory: Invalid arguments\n");
    return E_INVALIDARG;
  }

  // Validate DDS file in memory
  if (_ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
  {
    DebugTrace("LoadDDSTextureFromMemory: Data too small for DDS header\n");
    return E_FAIL;
  }

  const auto magicNumber = *reinterpret_cast<const uint32_t *>(_ddsData);
  if (magicNumber != DDS_MAGIC)
  {
    DebugTrace("LoadDDSTextureFromMemory: Invalid DDS magic number\n");
    return E_FAIL;
  }

  const auto header = reinterpret_cast<const DDS_HEADER *>(_ddsData + sizeof(uint32_t));

  // Verify header to validate DDS file
  if (header->size != sizeof(DDS_HEADER) || header->ddspf.size != sizeof(DDS_PIXELFORMAT))
  {
    DebugTrace("LoadDDSTextureFromMemory: Invalid DDS header size\n");
    return E_FAIL;
  }

  size_t offset = sizeof(uint32_t) + sizeof(DDS_HEADER);

  // Check for extensions
  if ((header->ddspf.flags & DDS_FOURCC) && (MAKE_FOUR_CC('D', 'X', '1', '0') == header->ddspf.fourCC))
  {
    if (_ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)))
    {
      DebugTrace("LoadDDSTextureFromMemory: Data too small for DXT10 header\n");
      return E_FAIL;
    }
    offset += sizeof(DDS_HEADER_DXT10);
  }

  return CreateTextureFromDDS(_d3dDevice, header, _ddsData + offset, _ddsDataSize - offset, _maxSize, D3D12_RESOURCE_FLAG_NONE, _texture, _subresources, _isCubeMap);
}

HRESULT Neuron::Graphics::LoadDDSTextureFromFile(ID3D12Device *_d3dDevice, const wchar_t *_fileName, ID3D12Resource **_texture, std::unique_ptr<uint8_t[]> &_ddsData, std::vector<D3D12_SUBRESOURCE_DATA> &_subresources, size_t _maxSize, DDS_ALPHA_MODE *_alphaMode, bool *_isCubeMap)
{
  if (_texture) *_texture = nullptr;
  if (_alphaMode) *_alphaMode = DDS_ALPHA_MODE_UNKNOWN;
  if (_isCubeMap) *_isCubeMap = false;

  if (!_d3dDevice || !_fileName || !_texture)
  {
    DebugTrace("LoadDDSTextureFromFile: Invalid arguments\n");
    return E_INVALIDARG;
  }

  // Use BinaryFile utility to read the file
  auto fileData = BinaryFile::ReadFile(_fileName);
  if (fileData.empty())
  {
    DebugTrace(L"LoadDDSTextureFromFile: Failed to read file: {}\n", _fileName);
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  const size_t len = fileData.size();
  if (len < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
  {
    DebugTrace(L"LoadDDSTextureFromFile: File too small for DDS: {}\n", _fileName);
    return E_FAIL;
  }

  // Transfer ownership to the output unique_ptr
  _ddsData.reset(new(std::nothrow) uint8_t[len]);
  if (!_ddsData)
  {
    DebugTrace("LoadDDSTextureFromFile: Out of memory\n");
    return E_OUTOFMEMORY;
  }
  std::memcpy(_ddsData.get(), fileData.data(), len);

  return LoadDDSTextureFromMemory(_d3dDevice, _ddsData.get(), len, _texture, _subresources, _maxSize, _alphaMode, _isCubeMap);
}