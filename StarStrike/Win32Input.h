#pragma once

namespace StarStrike
{
  // Win32-based input handler that replaces SDL input
  // Uses WndProcManager to receive keyboard/mouse events
  class Win32Input
  {
  public:
    // Initialize the input system
    static void Startup();
    
    // Shutdown the input system
    static void Shutdown();
    
    // Process pending input (called each frame)
    static void ProcessInput();
    
    // Get key state array (replaces SDL key array)
    static bool IsKeyDown(int vkCode);
    
    // Get last typed character (for text input)
    static char GetLastChar();
    static void ClearLastChar();

  private:
    // WndProc callback for input handling
    static LRESULT CALLBACK InputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // Convert Windows virtual key to game key code
    static int VKToGameKey(int vkCode);
    
    // Key state tracking
    static constexpr int MAX_KEYS = 256;
    inline static bool m_keyState[MAX_KEYS] = {};
    inline static bool m_keyPressed[MAX_KEYS] = {};  // Single press detection
    inline static char m_lastChar = 0;
    inline static bool m_initialized = false;
  };
}

using namespace StarStrike;
