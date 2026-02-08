#include "pch.h"
#include "Win32Input.h"
#include "WndProcManager.h"
#include "input.h"
#include "gfx.h"
#include "main.h"

// External game functions
extern void handle_key_event(int key, char ascii);

namespace StarStrike
{
  // Virtual key to game key mapping table
  struct VKMapping
  {
    int vkCode;
    int gameKey;
  };

  static const VKMapping g_vkMappings[] = {
    { VK_F1, KBD_F1 },
    { VK_F2, KBD_F2 },
    { VK_F3, KBD_F3 },
    { VK_F4, KBD_F4 },
    { VK_F5, KBD_F5 },
    { VK_F6, KBD_F6 },
    { VK_F7, KBD_F7 },
    { VK_F8, KBD_F8 },
    { VK_F9, KBD_F9 },
    { VK_F10, KBD_F10 },
    { VK_F11, KBD_F11 },
    { VK_F12, KBD_F12 },
    { 'Y', KBD_Y },
    { 'N', KBD_N },
    { 'A', KBD_FIRE },
    { VK_SPACE, KBD_FIRE },    // Space bar also fires
    { 'E', KBD_ECM },
    { VK_TAB, KBD_ENERGY_BOMB },
    { 'H', KBD_HYPERSPACE },
    { VK_LCONTROL, KBD_CTRL },
    { VK_RCONTROL, KBD_CTRL },
    { VK_CONTROL, KBD_CTRL },
    { 'J', KBD_JUMP },
    { VK_ESCAPE, KBD_ESCAPE },
    { 'C', KBD_DOCK },
    { 'D', KBD_D },
    { 'O', KBD_ORIGIN },
    { 'F', KBD_FIND },
    { 'M', KBD_FIRE_MISSILE },
    { 'T', KBD_TARGET_MISSILE },
    { 'U', KBD_UNARM_MISSILE },
    { 'P', KBD_PAUSE },
    { 'R', KBD_RESUME },
    { VK_OEM_PLUS, KBD_INC_SPEED },   // '+' key for speed up
    { VK_ADD, KBD_INC_SPEED },         // Numpad + for speed up
    { VK_OEM_MINUS, KBD_DEC_SPEED },  // '-' key for slow down
    { VK_SUBTRACT, KBD_DEC_SPEED },   // Numpad - for slow down
    { VK_OEM_2, KBD_DEC_SPEED },      // '/' key for slow down (legacy)
    { 'S', KBD_UP },
    { VK_UP, KBD_UP },
    { 'X', KBD_DOWN },
    { VK_DOWN, KBD_DOWN },
    { VK_OEM_COMMA, KBD_LEFT },
    { VK_LEFT, KBD_LEFT },
    { VK_OEM_PERIOD, KBD_RIGHT },
    { VK_RIGHT, KBD_RIGHT },
    { VK_RETURN, KBD_ENTER },
    { VK_BACK, KBD_BACKSPACE },
  };

  static const int g_numMappings = sizeof(g_vkMappings) / sizeof(g_vkMappings[0]);

  void Win32Input::Startup()
  {
    // Clear key states
    memset(m_keyState, 0, sizeof(m_keyState));
    memset(m_keyPressed, 0, sizeof(m_keyPressed));
    m_lastChar = 0;
    
    // Register our WndProc with the manager
    WndProcManager::AddWndProc(InputWndProc);
    
    m_initialized = true;
    DebugTrace("Win32Input initialized\n");
  }

  void Win32Input::Shutdown()
  {
    if (m_initialized)
    {
      WndProcManager::RemoveWndProc(InputWndProc);
      m_initialized = false;
      DebugTrace("Win32Input shutdown\n");
    }
  }

  void Win32Input::ProcessInput()
  {
    // Update the game's kbd[] array based on our key state
    memset(kbd, 0, sizeof(int) * KBD_LAST);
    
    for (int i = 0; i < g_numMappings; i++)
    {
      if (m_keyState[g_vkMappings[i].vkCode])
      {
        kbd[g_vkMappings[i].gameKey] = 1;
      }
    }
    
    // Clear single-press flags
    memset(m_keyPressed, 0, sizeof(m_keyPressed));
  }

  bool Win32Input::IsKeyDown(int vkCode)
  {
    if (vkCode >= 0 && vkCode < MAX_KEYS)
      return m_keyState[vkCode];
    return false;
  }

  char Win32Input::GetLastChar()
  {
    return m_lastChar;
  }

  void Win32Input::ClearLastChar()
  {
    m_lastChar = 0;
  }

  int Win32Input::VKToGameKey(int vkCode)
  {
    for (int i = 0; i < g_numMappings; i++)
    {
      if (g_vkMappings[i].vkCode == vkCode)
        return g_vkMappings[i].gameKey;
    }
    return KBD_NONE;
  }

  LRESULT CALLBACK Win32Input::InputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
      case WM_KEYDOWN:
      {
        int vkCode = static_cast<int>(wParam);
        if (vkCode >= 0 && vkCode < MAX_KEYS)
        {
          bool wasDown = m_keyState[vkCode];
          m_keyState[vkCode] = true;
          
          // Only trigger key event on first press (not repeat)
          if (!wasDown)
          {
            m_keyPressed[vkCode] = true;
            int gameKey = VKToGameKey(vkCode);
            handle_key_event(gameKey, m_lastChar);
          }
        }
        return 0;  // Handled
      }

      case WM_KEYUP:
      {
        int vkCode = static_cast<int>(wParam);
        if (vkCode >= 0 && vkCode < MAX_KEYS)
        {
          m_keyState[vkCode] = false;
        }
        return 0;  // Handled
      }

      case WM_CHAR:
      {
        // Store character for text input
        m_lastChar = static_cast<char>(wParam);
        
        // Re-trigger key event with the character
        // This handles text input for planet name search, etc.
        int gameKey = KBD_NONE;
        
        // Map common characters
        char upperChar = static_cast<char>(toupper(m_lastChar));
        if (isalpha(upperChar))
        {
          // Find the game key for this letter if any
          for (int i = 0; i < g_numMappings; i++)
          {
            if (g_vkMappings[i].vkCode == upperChar)
            {
              gameKey = g_vkMappings[i].gameKey;
              break;
            }
          }
        }
        
        handle_key_event(gameKey, m_lastChar);
        return 0;  // Handled
      }

      case WM_SIZE:
      {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width > 0 && height > 0)
        {
          gfx_resize_window(width, height);
        }
        return -1;  // Let other handlers also process this
      }

      case WM_CLOSE:
      {
        finish_game();
        return 0;  // Handled
      }

      default:
        break;
    }

    // Not handled by us - return -1 to let WndProcManager continue
    return -1;
  }
}
