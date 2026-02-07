#include "pch.h"
#include "OpenGLContext.h"
#include "GL/gl.h"

namespace StarStrike
{
  bool OpenGLContext::Startup(HWND hwnd)
  {
    m_hwnd = hwnd;
    m_hdc = ::GetDC(hwnd);
    
    if (!m_hdc)
    {
      Fatal("OpenGLContext: Failed to get device context");
      return false;
    }

    // Set up pixel format descriptor for OpenGL
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cBlueBits = 8;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (pixelFormat == 0)
    {
      Fatal("OpenGLContext: Failed to choose pixel format");
      return false;
    }

    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd))
    {
      Fatal("OpenGLContext: Failed to set pixel format");
      return false;
    }

    // Create OpenGL rendering context
    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc)
    {
      Fatal("OpenGLContext: Failed to create OpenGL context");
      return false;
    }

    // Make the context current
    if (!wglMakeCurrent(m_hdc, m_hglrc))
    {
      Fatal("OpenGLContext: Failed to make context current");
      wglDeleteContext(m_hglrc);
      m_hglrc = nullptr;
      return false;
    }

    DebugTrace("OpenGL Context initialized successfully\n");
    DebugTrace("OpenGL Version: {}\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    DebugTrace("OpenGL Vendor: {}\n", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    DebugTrace("OpenGL Renderer: {}\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    return true;
  }

  void OpenGLContext::Shutdown()
  {
    if (m_hglrc)
    {
      wglMakeCurrent(nullptr, nullptr);
      wglDeleteContext(m_hglrc);
      m_hglrc = nullptr;
    }

    if (m_hdc && m_hwnd)
    {
      ::ReleaseDC(m_hwnd, m_hdc);
      m_hdc = nullptr;
    }

    m_hwnd = nullptr;
    DebugTrace("OpenGL Context shutdown complete\n");
  }

  void OpenGLContext::SwapBuffers()
  {
    if (m_hdc)
    {
      ::SwapBuffers(m_hdc);
    }
  }

  bool OpenGLContext::MakeCurrent()
  {
    if (m_hdc && m_hglrc)
    {
      return wglMakeCurrent(m_hdc, m_hglrc) == TRUE;
    }
    return false;
  }
}
