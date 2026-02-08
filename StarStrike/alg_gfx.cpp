#include "pch.h"
#include "config.h"
#include "gfx.h"
#include "elite.h"
#include "space.h"
#include "threed.h"
#include "GdiBitmap.h"
#include "Rendering/DX12Renderer.h"
#include "Rendering/ShipRenderer.h"

#define PI 3.1415926535898

#define FILLED_CIRCLE    1
#define WIREFRAME_CIRCLE 2

std::unique_ptr<GdiBitmap> scanner_image;

int clip_tx;
int clip_ty;
int clip_bx;
int clip_by;

/*
 * Return the pixel value at (x, y) from a GdiBitmap
 */
uint32_t gfx_get_pixel(const GdiBitmap *bmp, int x, int y) { return GdiBitmapLoader::GetPixel(bmp, x, y); }

void gfx_get_char_size(const GdiBitmap *bmp, int x, int y, int *size_x, int *size_y)
{
  *size_x = 0;
  *size_y = 0;

  // Auto-detect mask convention by sampling the corner pixel (should be background)
  uint32_t background = gfx_get_pixel(bmp, (x * 32), (y * 32));

  for (int dy = 0; dy < 32; dy++)
  {
    for (int dx = 0; dx < 32; dx++)
    {
      uint32_t pixel = gfx_get_pixel(bmp, (x * 32) + dx, (y * 32) + dy);

      // If this pixel differs from background, it's a character pixel
      if (pixel != background)
      {
        if (dx > *size_x) *size_x = dx;
        if (dy > *size_y) *size_y = dy;
      }
    }
  }

  // Add 1 to convert from max index to size, ensure minimum spacing
  (*size_x)++;
  (*size_y)++;
}

std::unique_ptr<GdiBitmap> gfx_load_bitmap(const char *filename)
{
  auto fname = FileSys::GetHomeDirectoryA() + filename;
  auto bmap = GdiBitmapLoader::LoadBMP(fname);
  if (!bmap) Fatal("Failed to load bitmap '{}'", fname);
  return bmap;
}

int gfx_graphics_startup(void)
{
  // Set initial viewport and clip region
  gfx_set_clip_region(0, 0, 512, 512);

  scanner_image = gfx_load_bitmap(scanner_filename);

  if (!scanner_image) Fatal("Error reading scanner bitmap file: {}.\n", scanner_filename);

  // Sync the palette from the scanner bitmap to DX12Renderer
  if (scanner_image->palette && scanner_image->paletteSize > 0) StarStrike::DX12Renderer::SetPaletteFromRGBQUAD(scanner_image->palette.get(), scanner_image->paletteSize);

  // Register all legacy sprites with DX12Renderer
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_SCANNER_1, L"scanner.bmp", 0, 0, 128);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_SCANNER_2, L"scanner.bmp", 128, 0, 128);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_SCANNER_3, L"scanner.bmp", 256, 0, 128);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_SCANNER_4, L"scanner.bmp", 384, 0, 128);

  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_BLAKE, L"blake.bmp", 0, 1, 128);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_ELITE_TXT, L"elitetx3.bmp", 0, 0, 256);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_GREEN_DOT, L"greendot.bmp", 0, 0, 16);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_MISSILE_GREEN, L"missgrn.bmp", 0, 0, 16);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_MISSILE_RED, L"missred.bmp", 0, 0, 16);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_MISSILE_YELLOW, L"missyell.bmp", 0, 0, 16);

  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_BIG_E, L"ecm.bmp", 0, 0, 32);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_RED_DOT, L"reddot.bmp", 0, 0, 16);
  StarStrike::DX12Renderer::RegisterLegacySprite(IMG_BIG_S, L"safe.bmp", 0, 0, 32);

  // Load fonts for DX12 text rendering
  StarStrike::DX12Renderer::LoadFont(L"verd2.bmp", L"verd2msk.bmp");

  return 0;
}

void gfx_graphics_shutdown(void) { scanner_image.reset(); }

void gfx_plot_pixel(int x, int y, int col)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(col);
  StarStrike::DX12Renderer::DrawPixel(static_cast<float>(x + GFX_X_OFFSET), static_cast<float>(y + GFX_Y_OFFSET), color);
}

void gfx_draw_gl_circle(int cx, int cy, int radius, int circle_colour, int type)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(circle_colour);
  bool filled = (type == FILLED_CIRCLE);
  StarStrike::DX12Renderer::DrawCircle(static_cast<float>(cx + GFX_X_OFFSET), static_cast<float>(cy + GFX_Y_OFFSET), static_cast<float>(radius), color, filled);
}

void gfx_draw_filled_circle(int cx, int cy, int radius, int circle_colour) { gfx_draw_gl_circle(cx, cy, radius, circle_colour, FILLED_CIRCLE); }

void gfx_draw_circle(int cx, int cy, int radius, int circle_colour) { gfx_draw_gl_circle(cx, cy, radius, circle_colour, WIREFRAME_CIRCLE); }

void gfx_draw_line(int x1, int y1, int x2, int y2) { gfx_draw_colour_line(x1, y1, x2, y2, GFX_COL_WHITE); }

void gfx_draw_colour_line(int x1, int y1, int x2, int y2, int line_colour)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(line_colour);
  StarStrike::DX12Renderer::DrawLine(static_cast<float>(x1 + GFX_X_OFFSET), static_cast<float>(y1 + GFX_Y_OFFSET), static_cast<float>(x2 + GFX_X_OFFSET), static_cast<float>(y2 + GFX_Y_OFFSET), color);
}

void gfx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int col)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(col);
  StarStrike::DX12Renderer::DrawTriangle(static_cast<float>(x1 + GFX_X_OFFSET), static_cast<float>(y1 + GFX_Y_OFFSET), static_cast<float>(x2 + GFX_X_OFFSET), static_cast<float>(y2 + GFX_Y_OFFSET), static_cast<float>(x3 + GFX_X_OFFSET), static_cast<float>(y3 + GFX_Y_OFFSET), color);
}

void gfx_display_text(int x, int y, const char *txt) { gfx_display_colour_text(x, y, txt, GFX_COL_WHITE); }

void gfx_display_colour_text(int x, int y, const char *txt, int col)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(col);
  StarStrike::DX12Renderer::DrawText(static_cast<float>(x + GFX_X_OFFSET), static_cast<float>(y + GFX_Y_OFFSET), txt, color, false);
}

void gfx_display_centre_text(int y, const char *str, int psize, int col)
{
  bool large = (psize == 140);
  int txt_colour = large ? GFX_COL_WHITE : col;
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(txt_colour);
  StarStrike::DX12Renderer::DrawTextCentered(static_cast<float>((128 * GFX_SCALE) + GFX_X_OFFSET), static_cast<float>(y + GFX_Y_OFFSET), str, color, large);
}

void gfx_draw_rectangle(int tx, int ty, int bx, int by, int col)
{
  XMFLOAT4 color = StarStrike::DX12Renderer::PaletteToColor(col);
  StarStrike::DX12Renderer::DrawRectangle(static_cast<float>(tx + GFX_X_OFFSET), static_cast<float>(ty + GFX_Y_OFFSET), static_cast<float>(bx + GFX_X_OFFSET), static_cast<float>(by + GFX_Y_OFFSET), color);
}

void gfx_display_pretty_text(int tx, int ty, int bx, int by, const char *txt)
{
  char strbuf[100];
  char *bptr;

  int maxlen = (bx - tx) / 8;

  const char *str = txt;
  int len = strlen(txt);

  while (len > 0)
  {
    int pos = maxlen;
    if (pos > len) pos = len;

    while ((str[pos] != ' ') && (str[pos] != ',') && (str[pos] != '.') && (str[pos] != '\0')) { pos--; }

    len = len - pos - 1;

    for (bptr = strbuf; pos >= 0; pos--) *bptr++ = *str++;

    *bptr = '\0';

    gfx_display_colour_text(tx, ty, strbuf, GFX_COL_WHITE);
    ty += (8 * GFX_SCALE);
  }
}

void gfx_draw_scanner(void)
{
  // Draw scanner using 4 textures
  float scannerY = 385.0f + GFX_Y_OFFSET;
  float scannerX = static_cast<float>(GFX_X_OFFSET);

  int scannerSprites[4] = {IMG_SCANNER_1, IMG_SCANNER_2, IMG_SCANNER_3, IMG_SCANNER_4};

  for (int i = 0; i < 4; i++) { if (StarStrike::DX12Renderer::HasLegacySprite(scannerSprites[i])) StarStrike::DX12Renderer::DrawLegacySprite(scannerSprites[i], scannerX + (i * 128.0f), scannerY); }

  // Draw border lines
  gfx_draw_line(0, 1, 0, 384);
  gfx_draw_line(0, 1, 511, 1);
  gfx_draw_line(511, 1, 511, 384);
}

void gfx_set_clip_region(int tx, int ty, int bx, int by)
{
  clip_tx = tx;
  clip_ty = ty;
  clip_bx = bx;
  clip_by = by;

  StarStrike::DX12Renderer::SetClipRegion(tx, ty, bx, by);
}

void gfx_resize_window(int width, int height)
{
  // Size is now managed by ClientEngine::OutputSize()
  // Just refresh the clip region
  gfx_set_clip_region(clip_tx, clip_ty, clip_bx, clip_by);
}

void gfx_draw_sprite(int sprite_no, int x, int y)
{
  // Calculate x position for centered sprites
  float drawX = static_cast<float>(x);
  if (x == -1) drawX = static_cast<float>(((256 * GFX_SCALE) - 192) / 2);

  // Draw using DX12Renderer
  if (StarStrike::DX12Renderer::HasLegacySprite(sprite_no)) { StarStrike::DX12Renderer::DrawLegacySprite(sprite_no, drawX + GFX_X_OFFSET, static_cast<float>(y + GFX_Y_OFFSET)); }
}

void gfx_draw_view(void)
{
  struct univ_object copy;

  gfx_set_clip_region(1, 2, 510, 383);

  for (int i = 0; i < MAX_UNIV_OBJECTS; i++)
  {
    if ((universe[i].type == 0) || (universe[i].distance < 170)) continue;

    copy = universe[i];

    quaternion_slerp(&universe[i].oldquat, &universe[i].quat, timeslice, &copy.quat);
    quat_to_matrix(&copy.quat, copy.rotmat);

    copy.location.x = ((universe[i].location.x - universe[i].oldlocation.x) * timeslice) + universe[i].oldlocation.x;
    copy.location.y = ((universe[i].location.y - universe[i].oldlocation.y) * timeslice) + universe[i].oldlocation.y;
    copy.location.z = ((universe[i].location.z - universe[i].oldlocation.z) * timeslice) + universe[i].oldlocation.z;

    draw_ship(&copy);

    universe[i].flags &= ~FLG_FIRING;
  }

  gfx_set_clip_region(0, 0, 512, 512);
}

void gfx_advance_frame(void)
{
  for (int i = 0; i < MAX_UNIV_OBJECTS; i++)
  {
    universe[i].oldlocation = universe[i].location;
    universe[i].oldquat = universe[i].quat;
  }
}

void gfx_set_camera(int angle) { StarStrike::ShipRenderer::SetCamera(angle); }

void gfx_draw_gl_ship(struct univ_object *univ)
{
  // Use DX12 ship renderer
  StarStrike::ShipRenderer::SetWireframe(wireframe != 0);
  StarStrike::ShipRenderer::DrawShip(univ);
  StarStrike::ShipRenderer::DrawLaser(univ);
}