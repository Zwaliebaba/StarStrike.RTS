/**
 * GdiBitmap.h
 *
 * Win32 GDI bitmap wrapper to replace SDL_Surface for bitmap loading.
 * Phase 4 migration from SDL to native Win32.
 */

#pragma once

#include <Windows.h>
#include <string>
#include <memory>

namespace Neuron
{
  struct GdiBitmap
  {
    HBITMAP hBitmap{};
    HDC hDC{};
    int width{};
    int height{};
    int bitsPerPixel{};
    int pitch{};
    void* pixels{};
    std::unique_ptr<RGBQUAD[]> palette;
    int paletteSize{};

    GdiBitmap() = default;
    ~GdiBitmap() { Free(); }

    GdiBitmap(const GdiBitmap&) = delete;
    GdiBitmap& operator=(const GdiBitmap&) = delete;

    GdiBitmap(GdiBitmap&& _other) noexcept
        : hBitmap(_other.hBitmap),
          hDC(_other.hDC),
          width(_other.width),
          height(_other.height),
          bitsPerPixel(_other.bitsPerPixel),
          pitch(_other.pitch),
          pixels(_other.pixels),
          palette(std::move(_other.palette)),
          paletteSize(_other.paletteSize)
    {
      _other.hBitmap = nullptr;
      _other.hDC = nullptr;
      _other.pixels = nullptr;
    }

    GdiBitmap& operator=(GdiBitmap&& _other) noexcept
    {
      if (this != &_other)
      {
        Free();
        hBitmap = _other.hBitmap;
        hDC = _other.hDC;
        width = _other.width;
        height = _other.height;
        bitsPerPixel = _other.bitsPerPixel;
        pitch = _other.pitch;
        pixels = _other.pixels;
        palette = std::move(_other.palette);
        paletteSize = _other.paletteSize;
        _other.hBitmap = nullptr;
        _other.hDC = nullptr;
        _other.pixels = nullptr;
      }
      return *this;
    }

    void Free()
    {
      if (hBitmap)
      {
        DeleteObject(hBitmap);
        hBitmap = nullptr;
      }
      if (hDC)
      {
        DeleteDC(hDC);
        hDC = nullptr;
      }
      pixels = nullptr;
      palette.reset();
      paletteSize = 0;
    }

    [[nodiscard]] bool IsValid() const { return hBitmap != nullptr && pixels != nullptr; }
  };

  class GdiBitmapLoader
  {
  public:
    [[nodiscard]] static std::unique_ptr<GdiBitmap> LoadBMP(const std::string& _filename);
    [[nodiscard]] static std::unique_ptr<GdiBitmap> CreateRGB(int _width, int _height, int _bpp);
    static void Blit(GdiBitmap* _src, int _srcX, int _srcY, int _srcW, int _srcH, GdiBitmap* _dst, int _dstX, int _dstY);

    [[nodiscard]] static uint32_t GetPixel(const GdiBitmap* _bmp, int _x, int _y);
    static void SetPixel(GdiBitmap* _bmp, int _x, int _y, uint32_t _color);
  };
} // namespace Neuron
