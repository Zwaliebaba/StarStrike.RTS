#include "pch.h"
#include "DDSTextureLoader.h"
#include "FileSys.h"
#include "Debug.h"

#ifdef _MSC_VER
#pragma warning(disable : 4061 4062)
#endif

using namespace Neuron;
using namespace Neuron::Graphics;

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//--------------------------------------------------------------------------------------
namespace
{
#pragma pack(push, 1)

  constexpr uint32_t DDS_MAGIC = 0x20534444;  // "DDS "

  struct DdsPixelFormat
  {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
  };

  constexpr uint32_t DDS_FOURCC = 0x00000004;
  constexpr uint32_t DDS_RGB = 0x00000040;
  constexpr uint32_t DDS_LUMINANCE = 0x00020000;
  constexpr uint32_t DDS_ALPHA = 0x00000002;
  constexpr uint32_t DDS_BUMPDUDV = 0x00080000;
  constexpr uint32_t DDS_HEADER_FLAGS_VOLUME = 0x00800000;
  constexpr uint32_t DDS_HEIGHT = 0x00000002;
  constexpr uint32_t DDS_CUBEMAP = 0x00000200;
  constexpr uint32_t DDS_CUBEMAP_ALLFACES = 0x0000FC00;

  struct DdsHeader
  {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DdsPixelFormat ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
  };

  struct DdsHeaderDxt10
  {
    DXGI_FORMAT dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
  };

#pragma pack(pop)

  static_assert(sizeof(DdsPixelFormat) == 32, "DDS pixel format size mismatch");
  static_assert(sizeof(DdsHeader) == 124, "DDS Header size mismatch");
  static_assert(sizeof(DdsHeaderDxt10) == 20, "DDS DX10 Extended Header size mismatch");

  constexpr size_t DDS_MIN_HEADER_SIZE = sizeof(uint32_t) + sizeof(DdsHeader);
  constexpr size_t DDS_DX10_HEADER_SIZE = sizeof(uint32_t) + sizeof(DdsHeader) + sizeof(DdsHeaderDxt10);

  //--------------------------------------------------------------------------------------
  uint32_t CountMips(uint32_t _width, uint32_t _height) noexcept
  {
    if (_width == 0 || _height == 0)
      return 0;

    uint32_t count = 1;
    while (_width > 1 || _height > 1)
    {
      _width >>= 1;
      _height >>= 1;
      count++;
    }
    return count;
  }

  //--------------------------------------------------------------------------------------
  size_t BitsPerPixel(DXGI_FORMAT _format) noexcept
  {
    switch (_format)
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
      return 32;

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
    case DXGI_FORMAT_B4G4R4A4_UNORM:
      return 16;

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
  HRESULT GetSurfaceInfo(size_t _width, size_t _height, DXGI_FORMAT _format,
                         size_t* _outNumBytes, size_t* _outRowBytes, size_t* _outNumRows) noexcept
  {
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    bool bc = false;
    bool packed = false;
    size_t bpe = 0;

    switch (_format)
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
      packed = true;
      bpe = 4;
      break;

    default:
      break;
    }

    if (bc)
    {
      uint64_t numBlocksWide = 0;
      if (_width > 0)
        numBlocksWide = std::max<uint64_t>(1u, (static_cast<uint64_t>(_width) + 3u) / 4u);
      uint64_t numBlocksHigh = 0;
      if (_height > 0)
        numBlocksHigh = std::max<uint64_t>(1u, (static_cast<uint64_t>(_height) + 3u) / 4u);
      rowBytes = numBlocksWide * bpe;
      numRows = numBlocksHigh;
      numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed)
    {
      rowBytes = ((static_cast<uint64_t>(_width) + 1u) >> 1) * bpe;
      numRows = static_cast<uint64_t>(_height);
      numBytes = rowBytes * _height;
    }
    else
    {
      const size_t bpp = BitsPerPixel(_format);
      if (!bpp)
        return E_INVALIDARG;

      rowBytes = (static_cast<uint64_t>(_width) * bpp + 7u) / 8u;
      numRows = static_cast<uint64_t>(_height);
      numBytes = rowBytes * _height;
    }

    if (_outNumBytes)
      *_outNumBytes = static_cast<size_t>(numBytes);
    if (_outRowBytes)
      *_outRowBytes = static_cast<size_t>(rowBytes);
    if (_outNumRows)
      *_outNumRows = static_cast<size_t>(numRows);

    return S_OK;
  }

  //--------------------------------------------------------------------------------------
#define ISBITMASK(r, g, b, a) (ddpf.rBitMask == r && ddpf.gBitMask == g && ddpf.bBitMask == b && ddpf.aBitMask == a)

  DXGI_FORMAT GetDxgiFormat(const DdsPixelFormat& ddpf) noexcept
  {
    if (ddpf.flags & DDS_RGB)
    {
      switch (ddpf.rgbBitCount)
      {
      case 32:
        if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
          return DXGI_FORMAT_R8G8B8A8_UNORM;
        if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
          return DXGI_FORMAT_B8G8R8A8_UNORM;
        if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0))
          return DXGI_FORMAT_B8G8R8X8_UNORM;
        if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
          return DXGI_FORMAT_R10G10B10A2_UNORM;
        if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
          return DXGI_FORMAT_R16G16_UNORM;
        if (ISBITMASK(0xffffffff, 0, 0, 0))
          return DXGI_FORMAT_R32_FLOAT;
        break;

      case 16:
        if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
          return DXGI_FORMAT_B5G5R5A1_UNORM;
        if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0))
          return DXGI_FORMAT_B5G6R5_UNORM;
        if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
          return DXGI_FORMAT_B4G4R4A4_UNORM;
        if (ISBITMASK(0x00ff, 0, 0, 0xff00))
          return DXGI_FORMAT_R8G8_UNORM;
        if (ISBITMASK(0xffff, 0, 0, 0))
          return DXGI_FORMAT_R16_UNORM;
        break;

      case 8:
        if (ISBITMASK(0xff, 0, 0, 0))
          return DXGI_FORMAT_R8_UNORM;
        break;
      }
    }
    else if (ddpf.flags & DDS_LUMINANCE)
    {
      if (16 == ddpf.rgbBitCount)
      {
        if (ISBITMASK(0xffff, 0, 0, 0))
          return DXGI_FORMAT_R16_UNORM;
        if (ISBITMASK(0x00ff, 0, 0, 0xff00))
          return DXGI_FORMAT_R8G8_UNORM;
      }
      else if (8 == ddpf.rgbBitCount)
      {
        if (ISBITMASK(0xff, 0, 0, 0))
          return DXGI_FORMAT_R8_UNORM;
      }
    }
    else if (ddpf.flags & DDS_ALPHA)
    {
      if (8 == ddpf.rgbBitCount)
        return DXGI_FORMAT_A8_UNORM;
    }
    else if (ddpf.flags & DDS_FOURCC)
    {
      if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
        return DXGI_FORMAT_BC1_UNORM;
      if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
        return DXGI_FORMAT_BC2_UNORM;
      if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
        return DXGI_FORMAT_BC3_UNORM;
      if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
        return DXGI_FORMAT_BC2_UNORM;
      if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
        return DXGI_FORMAT_BC3_UNORM;
      if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
        return DXGI_FORMAT_BC4_UNORM;
      if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
        return DXGI_FORMAT_BC4_UNORM;
      if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
        return DXGI_FORMAT_BC4_SNORM;
      if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
        return DXGI_FORMAT_BC5_UNORM;
      if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
        return DXGI_FORMAT_BC5_UNORM;
      if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
        return DXGI_FORMAT_BC5_SNORM;

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

#undef ISBITMASK

  //--------------------------------------------------------------------------------------
  DXGI_FORMAT MakeSrgb(DXGI_FORMAT _format) noexcept
  {
    switch (_format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_BC1_UNORM:
      return DXGI_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:
      return DXGI_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:
      return DXGI_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
      return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM:
      return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
      return _format;
    }
  }

  //--------------------------------------------------------------------------------------
  DXGI_FORMAT MakeLinear(DXGI_FORMAT _format) noexcept
  {
    switch (_format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB:
      return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB:
      return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB:
      return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return DXGI_FORMAT_BC7_UNORM;
    default:
      return _format;
    }
  }

  //--------------------------------------------------------------------------------------
  HRESULT FillInitData(
    size_t _width, size_t _height, size_t _depth, size_t _mipCount, size_t _arraySize,
    DXGI_FORMAT _format, size_t _maxSize, size_t _bitSize, const uint8_t* _bitData,
    size_t& _outWidth, size_t& _outHeight, size_t& _outDepth, size_t& _outSkipMip,
    std::vector<D3D12_SUBRESOURCE_DATA>& _outInitData)
  {
    if (!_bitData)
      return E_POINTER;

    _outSkipMip = 0;
    _outWidth = 0;
    _outHeight = 0;
    _outDepth = 0;

    size_t numBytes = 0;
    size_t rowBytes = 0;
    const uint8_t* pEndBits = _bitData + _bitSize;

    _outInitData.clear();

    const uint8_t* pSrcBits = _bitData;

    for (size_t j = 0; j < _arraySize; j++)
    {
      size_t w = _width;
      size_t h = _height;
      size_t d = _depth;

      for (size_t i = 0; i < _mipCount; i++)
      {
        HRESULT hr = GetSurfaceInfo(w, h, _format, &numBytes, &rowBytes, nullptr);
        if (FAILED(hr))
          return hr;

        if ((_mipCount <= 1) || !_maxSize || (w <= _maxSize && h <= _maxSize && d <= _maxSize))
        {
          if (!_outWidth)
          {
            _outWidth = w;
            _outHeight = h;
            _outDepth = d;
          }

          D3D12_SUBRESOURCE_DATA res = {pSrcBits, static_cast<LONG_PTR>(rowBytes), static_cast<LONG_PTR>(numBytes)};
          _outInitData.emplace_back(res);
        }
        else if (!j)
        {
          ++_outSkipMip;
        }

        if (pSrcBits + (numBytes * d) > pEndBits)
          return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        pSrcBits += numBytes * d;

        w = w >> 1;
        h = h >> 1;
        d = d >> 1;
        if (w == 0)
          w = 1;
        if (h == 0)
          h = 1;
        if (d == 0)
          d = 1;
      }
    }

    return _outInitData.empty() ? E_FAIL : S_OK;
  }

  //--------------------------------------------------------------------------------------
  HRESULT CreateTextureResource(
    ID3D12Device* _device, D3D12_RESOURCE_DIMENSION _resDim,
    size_t _width, size_t _height, size_t _depth, size_t _mipCount, size_t _arraySize,
    DXGI_FORMAT _format, D3D12_RESOURCE_FLAGS _resFlags, DdsLoaderFlags _loadFlags,
    ID3D12Resource** _outTexture) noexcept
  {
    if (!_device)
      return E_POINTER;

    if (static_cast<uint32_t>(_loadFlags & DdsLoaderFlags::ForceSRGB))
      _format = MakeSrgb(_format);
    else if (static_cast<uint32_t>(_loadFlags & DdsLoaderFlags::IgnoreSRGB))
      _format = MakeLinear(_format);

    D3D12_RESOURCE_DESC desc = {};
    desc.Width = static_cast<UINT>(_width);
    desc.Height = static_cast<UINT>(_height);
    desc.MipLevels = static_cast<UINT16>(_mipCount);
    desc.DepthOrArraySize = (_resDim == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
      ? static_cast<UINT16>(_depth)
      : static_cast<UINT16>(_arraySize);
    desc.Format = _format;
    desc.Flags = _resFlags;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Dimension = _resDim;

    const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = _device->CreateCommittedResource(
      &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(_outTexture));

    if (SUCCEEDED(hr) && *_outTexture)
      (*_outTexture)->SetName(L"DDSTexture");

    return hr;
  }

  //--------------------------------------------------------------------------------------
  HRESULT CreateTextureFromDds(
    ID3D12Device* _device, const DdsHeader* _header,
    const uint8_t* _bitData, size_t _bitSize, size_t _maxSize,
    D3D12_RESOURCE_FLAGS _resFlags, DdsLoaderFlags _loadFlags,
    ID3D12Resource** _outTexture, std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources,
    bool* _outIsCubeMap)
  {
    UINT width = _header->width;
    UINT height = _header->height;
    UINT depth = _header->depth;

    D3D12_RESOURCE_DIMENSION resDim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
    UINT arraySize = 1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isCubeMap = false;

    size_t mipCount = _header->mipMapCount;
    if (0 == mipCount)
      mipCount = 1;

    if ((_header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == _header->ddspf.fourCC))
    {
      auto d3d10ext = reinterpret_cast<const DdsHeaderDxt10*>(
        reinterpret_cast<const char*>(_header) + sizeof(DdsHeader));

      arraySize = d3d10ext->arraySize;
      if (arraySize == 0)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

      if (BitsPerPixel(d3d10ext->dxgiFormat) == 0)
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

      format = d3d10ext->dxgiFormat;

      switch (d3d10ext->resourceDimension)
      {
      case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if ((_header->flags & DDS_HEIGHT) && height != 1)
          return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        height = depth = 1;
        break;

      case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (d3d10ext->miscFlag & 0x4)
        {
          arraySize *= 6;
          isCubeMap = true;
        }
        depth = 1;
        break;

      case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        if (!(_header->flags & DDS_HEADER_FLAGS_VOLUME))
          return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if (arraySize > 1)
          return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        break;

      default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
      }

      resDim = static_cast<D3D12_RESOURCE_DIMENSION>(d3d10ext->resourceDimension);
    }
    else
    {
      format = GetDxgiFormat(_header->ddspf);

      if (format == DXGI_FORMAT_UNKNOWN)
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

      if (_header->flags & DDS_HEADER_FLAGS_VOLUME)
      {
        resDim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
      }
      else
      {
        if (_header->caps2 & DDS_CUBEMAP)
        {
          if ((_header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

          arraySize = 6;
          isCubeMap = true;
        }

        depth = 1;
        resDim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      }
    }

    if (mipCount > D3D12_REQ_MIP_LEVELS)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    if (_outIsCubeMap)
      *_outIsCubeMap = isCubeMap;

    size_t numberOfResources = (resDim == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : arraySize;
    numberOfResources *= mipCount;

    if (numberOfResources > D3D12_REQ_SUBRESOURCES)
      return E_INVALIDARG;

    _outSubresources.reserve(numberOfResources);

    size_t skipMip = 0;
    size_t twidth = 0;
    size_t theight = 0;
    size_t tdepth = 0;

    HRESULT hr = FillInitData(width, height, depth, mipCount, arraySize, format,
                              _maxSize, _bitSize, _bitData,
                              twidth, theight, tdepth, skipMip, _outSubresources);

    if (SUCCEEDED(hr))
    {
      size_t reservedMips = mipCount;
      if (static_cast<uint32_t>(_loadFlags & DdsLoaderFlags::MipReserve))
        reservedMips = std::min<size_t>(D3D12_REQ_MIP_LEVELS, CountMips(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));

      hr = CreateTextureResource(_device, resDim, twidth, theight, tdepth,
                                 reservedMips - skipMip, arraySize, format,
                                 _resFlags, _loadFlags, _outTexture);
    }

    if (FAILED(hr))
      _outSubresources.clear();

    return hr;
  }

  //--------------------------------------------------------------------------------------
  DdsAlphaMode GetAlphaMode(const DdsHeader* _header) noexcept
  {
    if (_header->ddspf.flags & DDS_FOURCC)
    {
      if (MAKEFOURCC('D', 'X', '1', '0') == _header->ddspf.fourCC)
      {
        auto d3d10ext = reinterpret_cast<const DdsHeaderDxt10*>(
          reinterpret_cast<const uint8_t*>(_header) + sizeof(DdsHeader));
        auto mode = static_cast<DdsAlphaMode>(d3d10ext->miscFlags2 & 0x7);
        switch (mode)
        {
        case DdsAlphaMode::Straight:
        case DdsAlphaMode::Premultiplied:
        case DdsAlphaMode::Opaque:
        case DdsAlphaMode::Custom:
          return mode;
        default:
          break;
        }
      }
      else if ((MAKEFOURCC('D', 'X', 'T', '2') == _header->ddspf.fourCC) ||
               (MAKEFOURCC('D', 'X', 'T', '4') == _header->ddspf.fourCC))
      {
        return DdsAlphaMode::Premultiplied;
      }
    }

    return DdsAlphaMode::Unknown;
  }
}  // anonymous namespace

//--------------------------------------------------------------------------------------
// Public API Implementation
//--------------------------------------------------------------------------------------

HRESULT DdsTextureLoader::LoadFromMemory(
  ID3D12Device* _device,
  const uint8_t* _ddsData,
  size_t _ddsDataSize,
  ID3D12Resource** _outTexture,
  std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources,
  size_t _maxSize,
  DdsAlphaMode* _outAlphaMode,
  bool* _outIsCubeMap)
{
  return LoadFromMemoryEx(_device, _ddsData, _ddsDataSize, _maxSize,
                          D3D12_RESOURCE_FLAG_NONE, DdsLoaderFlags::Default,
                          _outTexture, _outSubresources, _outAlphaMode, _outIsCubeMap);
}

HRESULT DdsTextureLoader::LoadFromMemoryEx(
  ID3D12Device* _device,
  const uint8_t* _ddsData,
  size_t _ddsDataSize,
  size_t _maxSize,
  D3D12_RESOURCE_FLAGS _resourceFlags,
  DdsLoaderFlags _loaderFlags,
  ID3D12Resource** _outTexture,
  std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources,
  DdsAlphaMode* _outAlphaMode,
  bool* _outIsCubeMap)
{
  if (_outTexture)
    *_outTexture = nullptr;
  if (_outAlphaMode)
    *_outAlphaMode = DdsAlphaMode::Unknown;
  if (_outIsCubeMap)
    *_outIsCubeMap = false;

  if (!_device || !_ddsData || !_outTexture)
    return E_INVALIDARG;

  if (_ddsDataSize < DDS_MIN_HEADER_SIZE)
    return E_FAIL;

  // Validate magic number
  auto dwMagicNumber = *reinterpret_cast<const uint32_t*>(_ddsData);
  if (dwMagicNumber != DDS_MAGIC)
    return E_FAIL;

  auto header = reinterpret_cast<const DdsHeader*>(_ddsData + sizeof(uint32_t));

  // Validate header
  if (header->size != sizeof(DdsHeader) || header->ddspf.size != sizeof(DdsPixelFormat))
    return E_FAIL;

  // Check for DX10 extension
  bool hasDxt10Header = false;
  if ((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
  {
    if (_ddsDataSize < DDS_DX10_HEADER_SIZE)
      return E_FAIL;
    hasDxt10Header = true;
  }

  auto offset = DDS_MIN_HEADER_SIZE + (hasDxt10Header ? sizeof(DdsHeaderDxt10) : 0u);
  const uint8_t* bitData = _ddsData + offset;
  size_t bitSize = _ddsDataSize - offset;

  HRESULT hr = CreateTextureFromDds(_device, header, bitData, bitSize, _maxSize,
                                    _resourceFlags, _loaderFlags,
                                    _outTexture, _outSubresources, _outIsCubeMap);

  if (SUCCEEDED(hr) && _outAlphaMode)
    *_outAlphaMode = GetAlphaMode(header);

  return hr;
}


HRESULT DdsTextureLoader::LoadFromFile(
  ID3D12Device* _device,
  const std::wstring& _filePath,
  ID3D12Resource** _outTexture,
  std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources,
  std::vector<uint8_t>& _outFileData,
  size_t _maxSize,
  DdsAlphaMode* _outAlphaMode,
  bool* _outIsCubeMap)
{
  return LoadFromFileEx(_device, _filePath, _maxSize,
                        D3D12_RESOURCE_FLAG_NONE, DdsLoaderFlags::Default,
                        _outTexture, _outSubresources, _outFileData, _outAlphaMode, _outIsCubeMap);
}

HRESULT DdsTextureLoader::LoadFromFileEx(
  ID3D12Device* _device,
  const std::wstring& _filePath,
  size_t _maxSize,
  D3D12_RESOURCE_FLAGS _resourceFlags,
  DdsLoaderFlags _loaderFlags,
  ID3D12Resource** _outTexture,
  std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources,
  std::vector<uint8_t>& _outFileData,
  DdsAlphaMode* _outAlphaMode,
  bool* _outIsCubeMap)
{
  if (_outTexture)
    *_outTexture = nullptr;
  if (_outAlphaMode)
    *_outAlphaMode = DdsAlphaMode::Unknown;
  if (_outIsCubeMap)
    *_outIsCubeMap = false;

  if (!_device || _filePath.empty() || !_outTexture)
    return E_INVALIDARG;

  // Read file into caller's buffer - caller must keep alive until upload completes
  _outFileData = BinaryFile::ReadFile(_filePath);

  if (_outFileData.empty())
  {
    DebugTrace(L"DdsTextureLoader: Failed to load file: {}\n", _filePath);
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  DebugTrace(L"DdsTextureLoader: Loaded {} bytes from {}\n", _outFileData.size(), _filePath);

  return LoadFromMemoryEx(_device, _outFileData.data(), _outFileData.size(),
                          _maxSize, _resourceFlags, _loaderFlags,
                          _outTexture, _outSubresources, _outAlphaMode, _outIsCubeMap);
}
