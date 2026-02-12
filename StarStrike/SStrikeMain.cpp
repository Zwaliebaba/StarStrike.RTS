#include "pch.h"
#include "SStrikeMain.h"
#include "gfx.h"
#include "elite.h"
#include "sound.h"
#include "input.h"
#include "file.h"
#include "Canvas.h"
#include "Rendering/ShipRenderer.h"

// External declarations for game functions and variables
extern int frame_count;
extern int finish;
extern void reset_game();
extern int ready_to_draw();
extern void draw_screen();
extern void gfx_advance_frame();
extern void handle_events();
extern void handle_keyboard_state();
extern void update_screen();

using namespace Neuron::Graphics;

void SStrikeMain::Startup()
{
  input_startup();
  read_config_file();

  // Initialize ShipRenderer for 3D ship rendering
  StarStrike::ShipRenderer::Startup();

  gfx_graphics_startup();
  snd_sound_startup();

  frame_count = 0;
  finish = 0;

  reset_game();
}

void SStrikeMain::Shutdown()
{
  snd_sound_shutdown();
  StarStrike::ShipRenderer::Shutdown();
  input_shutdown();
  gfx_graphics_shutdown();
}

void SStrikeMain::CreateDeviceDependentResources() { Neuron::GameMain::CreateDeviceDependentResources(); }

void SStrikeMain::CreateWindowSizeDependentResources()
{
  Neuron::GameMain::CreateWindowSizeDependentResources();

  // Notify Canvas of the new backbuffer size
  RECT outputSize = Graphics::Core::GetOutputSize();
  uint32_t width = static_cast<uint32_t>(outputSize.right - outputSize.left);
  uint32_t height = static_cast<uint32_t>(outputSize.bottom - outputSize.top);
  Canvas::OnResize(width, height);
}

void SStrikeMain::ReleaseDeviceDependentResources() { Neuron::GameMain::ReleaseDeviceDependentResources(); }

void SStrikeMain::ReleaseWindowSizeDependentResources() { Neuron::GameMain::ReleaseWindowSizeDependentResources(); }

void SStrikeMain::Update(float _deltaT)
{
  snd_update_sound();

  if (ready_to_draw())
    draw_screen();

  while (frame_count > 0)
  {
    gfx_advance_frame();
    handle_events();
    handle_keyboard_state();
    update_screen();
    frame_count--;
  }

  // Signal quit when game wants to exit
  if (finish)
    PostQuitMessage(0);
}

void SStrikeMain::Render()
{
  // Rendering handled by draw_screen() in Update
}