/**
 * GdiBitmap.cpp
 *
 * Win32 GDI bitmap implementation to replace SDL_Surface.
 * Phase 4 migration from SDL to native Win32.
 */

#include "pch.h"
#include "GdiBitmap.h"

namespace Neuron
{
  std::unique_ptr<GdiBitmap> GdiBitmapLoader::LoadBMP(const std::string& _filename)
  {
    auto bmp = std::make_unique<GdiBitmap>();

    // Load the bitmap file
    HANDLE hFile = CreateFileA(_filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
      return nullptr;
    }

    DWORD bytesRead = 0;

    // Read BITMAPFILEHEADER
    BITMAPFILEHEADER bmfh;
    if (!ReadFile(hFile, &bmfh, sizeof(BITMAPFILEHEADER), &bytesRead, nullptr) || bytesRead != sizeof(BITMAPFILEHEADER))
    {
      CloseHandle(hFile);
      return nullptr;
    }

    if (bmfh.bfType != 0x4D42) // 'BM'
    {
      CloseHandle(hFile);
      return nullptr;
    }

    // Read BITMAPINFOHEADER
    BITMAPINFOHEADER bmih;
    if (!ReadFile(hFile, &bmih, sizeof(BITMAPINFOHEADER), &bytesRead, nullptr) || bytesRead != sizeof(BITMAPINFOHEADER))
    {
      CloseHandle(hFile);
      return nullptr;
    }

    bmp->width = bmih.biWidth;
    bmp->height = abs(bmih.biHeight);
    bmp->bitsPerPixel = bmih.biBitCount;

    // Calculate palette size for indexed bitmaps
    int paletteEntries = 0;
    if (bmih.biBitCount <= 8)
    {
      paletteEntries = (bmih.biClrUsed > 0) ? bmih.biClrUsed : (1 << bmih.biBitCount);
      bmp->palette = std::make_unique<RGBQUAD[]>(paletteEntries);
      bmp->paletteSize = paletteEntries;

      if (!ReadFile(hFile, bmp->palette.get(), paletteEntries * sizeof(RGBQUAD), &bytesRead, nullptr))
      {
        CloseHandle(hFile);
        return nullptr;
      }
    }

    // Calculate row pitch (rows are DWORD-aligned in BMP files)
    int rowBytes = ((bmih.biWidth * bmih.biBitCount + 31) / 32) * 4;
    bmp->pitch = rowBytes;

    // Create a DIB section to hold the pixel data
    BITMAPINFO* pBmi = nullptr;
    size_t bmiSize = sizeof(BITMAPINFOHEADER) + paletteEntries * sizeof(RGBQUAD);
    std::vector<uint8_t> bmiBuffer(bmiSize);
    pBmi = reinterpret_cast<BITMAPINFO*>(bmiBuffer.data());
    pBmi->bmiHeader = bmih;
    pBmi->bmiHeader.biHeight = -abs(bmih.biHeight); // Top-down DIB

    if (paletteEntries > 0)
    {
      memcpy(pBmi->bmiColors, bmp->palette.get(), paletteEntries * sizeof(RGBQUAD));
    }

    HDC hScreenDC = GetDC(nullptr);
    bmp->hDC = CreateCompatibleDC(hScreenDC);
    bmp->hBitmap = CreateDIBSection(bmp->hDC, pBmi, DIB_RGB_COLORS, &bmp->pixels, nullptr, 0);
    ReleaseDC(nullptr, hScreenDC);

    if (!bmp->hBitmap || !bmp->pixels)
    {
      CloseHandle(hFile);
      return nullptr;
    }

    SelectObject(bmp->hDC, bmp->hBitmap);

    // Read pixel data - BMPs are stored bottom-up, but we created top-down DIB
    // Seek to pixel data offset
    SetFilePointer(hFile, bmfh.bfOffBits, nullptr, FILE_BEGIN);

    // Read bottom-up BMP and flip to top-down
    std::vector<uint8_t> rowBuffer(rowBytes);
    uint8_t* pixels = static_cast<uint8_t*>(bmp->pixels);

    if (bmih.biHeight > 0)
    {
      // Bottom-up BMP - read rows in reverse order
      for (int y = bmp->height - 1; y >= 0; y--)
      {
        if (!ReadFile(hFile, pixels + y * rowBytes, rowBytes, &bytesRead, nullptr))
        {
          CloseHandle(hFile);
          bmp->Free();
          return nullptr;
        }
      }
    }
    else
    {
      // Top-down BMP - read directly
      DWORD totalBytes = rowBytes * bmp->height;
      if (!ReadFile(hFile, bmp->pixels, totalBytes, &bytesRead, nullptr))
      {
        CloseHandle(hFile);
        bmp->Free();
        return nullptr;
      }
    }

    CloseHandle(hFile);
    return bmp;
  }

  std::unique_ptr<GdiBitmap> GdiBitmapLoader::CreateRGB(int _width, int _height, int _bpp)
  {
    auto bmp = std::make_unique<GdiBitmap>();

    bmp->width = _width;
    bmp->height = _height;
    bmp->bitsPerPixel = _bpp;
    bmp->pitch = ((_width * _bpp + 31) / 32) * 4;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = _width;
    bmi.bmiHeader.biHeight = -_height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = static_cast<WORD>(_bpp);
    bmi.bmiHeader.biCompression = BI_RGB;


    HDC hScreenDC = GetDC(nullptr);
    bmp->hDC = CreateCompatibleDC(hScreenDC);
    bmp->hBitmap = CreateDIBSection(bmp->hDC, &bmi, DIB_RGB_COLORS, &bmp->pixels, nullptr, 0);
    ReleaseDC(nullptr, hScreenDC);

    if (!bmp->hBitmap || !bmp->pixels)
    {
      bmp->Free();
      return nullptr;
    }

    SelectObject(bmp->hDC, bmp->hBitmap);

    // Clear to black
    memset(bmp->pixels, 0, bmp->pitch * _height);

    return bmp;
  }

  void GdiBitmapLoader::Blit(GdiBitmap* _src, int _srcX, int _srcY, int _srcW, int _srcH, GdiBitmap* _dst, int _dstX, int _dstY)
  {
    if (!_src || !_dst || !_src->IsValid() || !_dst->IsValid()) return;

    // Handle palette-to-RGB conversion manually for 8-bit paletted sources
    if (_src->bitsPerPixel == 8 && _src->palette && _dst->bitsPerPixel == 24)
    {
      for (int dy = 0; dy < _srcH; dy++)
      {
        int srcY = _srcY + dy;
        int dstY = _dstY + dy;

        if (srcY < 0 || srcY >= _src->height || dstY < 0 || dstY >= _dst->height) continue;

        for (int dx = 0; dx < _srcW; dx++)
        {
          int srcX = _srcX + dx;
          int dstX = _dstX + dx;

          if (srcX < 0 || srcX >= _src->width || dstX < 0 || dstX >= _dst->width) continue;

          // Get palette index from source
          const uint8_t* srcRow = static_cast<const uint8_t*>(_src->pixels) + srcY * _src->pitch;
          uint8_t paletteIndex = srcRow[srcX];

          // Get RGB from palette
          const RGBQUAD& color = _src->palette[paletteIndex];

          // Write to destination (BGR order for Windows DIB)
          uint8_t* dstRow = static_cast<uint8_t*>(_dst->pixels) + dstY * _dst->pitch;
          uint8_t* dstPixel = dstRow + dstX * 3;
          dstPixel[0] = color.rgbBlue;
          dstPixel[1] = color.rgbGreen;
          dstPixel[2] = color.rgbRed;
        }
      }
    }
    else
    {
      // Use BitBlt for compatible formats
      BitBlt(_dst->hDC, _dstX, _dstY, _srcW, _srcH, _src->hDC, _srcX, _srcY, SRCCOPY);
      GdiFlush();
    }
  }


  uint32_t GdiBitmapLoader::GetPixel(const GdiBitmap* _bmp, int _x, int _y)
  {
    if (!_bmp || !_bmp->pixels || _x < 0 || _y < 0 || _x >= _bmp->width || _y >= _bmp->height) return 0;

    const uint8_t* pixels = static_cast<const uint8_t*>(_bmp->pixels);
    const uint8_t* row = pixels + _y * _bmp->pitch;

    switch (_bmp->bitsPerPixel)
    {
      case 1:
      {
        int byteIndex = _x / 8;
        int bitIndex = 7 - (_x % 8);  // MSB first
        return (row[byteIndex] >> bitIndex) & 0x01;
      }

      case 4:
      {
        int byteIndex = _x / 2;
        if (_x % 2 == 0)
          return (row[byteIndex] >> 4) & 0x0F;  // High nibble
        else
          return row[byteIndex] & 0x0F;          // Low nibble
      }

      case 8:
        return row[_x];

      case 24:
      {
        const uint8_t* p = row + _x * 3;
        return p[0] | (p[1] << 8) | (p[2] << 16);
      }

      case 32:
        return reinterpret_cast<const uint32_t*>(row)[_x];

      default:
        return 0;
    }
  }

  void GdiBitmapLoader::SetPixel(GdiBitmap* _bmp, int _x, int _y, uint32_t _color)
  {
    if (!_bmp || !_bmp->pixels || _x < 0 || _y < 0 || _x >= _bmp->width || _y >= _bmp->height) return;

    uint8_t* pixels = static_cast<uint8_t*>(_bmp->pixels);
    uint8_t* row = pixels + _y * _bmp->pitch;

    switch (_bmp->bitsPerPixel)
    {
      case 8:
        row[_x] = static_cast<uint8_t>(_color);
        break;

      case 24:
      {
        uint8_t* p = row + _x * 3;
        p[0] = static_cast<uint8_t>(_color & 0xFF);
        p[1] = static_cast<uint8_t>((_color >> 8) & 0xFF);
        p[2] = static_cast<uint8_t>((_color >> 16) & 0xFF);
        break;
      }

      case 32:
        reinterpret_cast<uint32_t*>(row)[_x] = _color;
        break;
    }
  }
} // namespace Neuron
