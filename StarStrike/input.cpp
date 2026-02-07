#include "pch.h"
#include "input.h"
#include "main.h"
#include "gfx.h"
#include "Win32Input.h"

// Game keyboard state array (used by game logic)
int kbd[KBD_LAST];

int input_startup(void)
{
  // Initialize keyboard state
  memset(kbd, 0, sizeof(kbd));
  
  // Initialize Win32 input system
  Win32Input::Startup();
  
  return 0;
}

int input_shutdown(void)
{
  Win32Input::Shutdown();
  return 0;
}

void handle_events(void)
{
  // Process Win32 messages (done in main message loop)
  // Update game kbd[] state from Win32Input
  Win32Input::ProcessInput();
}
