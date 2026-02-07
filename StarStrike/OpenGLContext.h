#pragma once

namespace StarStrike
{
  // Manages Win32 OpenGL (WGL) context creation and management
  // Replaces SDL's OpenGL context handling
  class OpenGLContext
  {
  public:
    // Initialize OpenGL context on the given window
    static bool Startup(HWND hwnd);
    
    // Shutdown and release OpenGL context
    static void Shutdown();
    
    // Swap front and back buffers (replaces SDL_GL_SwapBuffers)
    static void SwapBuffers();
    
    // Make this context current
    static bool MakeCurrent();
    
    // Get the device context
    static HDC GetDC() { return m_hdc; }
    
    // Get the OpenGL rendering context
    static HGLRC GetGLRC() { return m_hglrc; }

  private:
    inline static HWND m_hwnd = nullptr;
    inline static HDC m_hdc = nullptr;
    inline static HGLRC m_hglrc = nullptr;
  };
}

using namespace StarStrike;
