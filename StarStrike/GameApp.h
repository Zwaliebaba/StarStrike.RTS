#pragma once

#include "Camera.h"
#include "Canvas.h"
#include "GuiWindow.h"
#include "ClientWorld.h"
#include "WorldRenderer.h"

namespace Neuron
{
  class GameApp : public GameMain
  {
  public:
    void Startup() override;
    void Shutdown() override;
    void Update(float _deltaT) override;
    void Render() override;

  private:
    void SetupIsometricCamera();
    void HandleInput();
    XMFLOAT3 ScreenToWorld(float _screenX, float _screenY) const;

    static LRESULT CALLBACK WndProc(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam);

    Camera         m_camera;
    ClientWorld    m_clientWorld;
    WorldRenderer  m_worldRenderer;
    SkyBox         m_skyBox;
    bool           m_connected       = false;
    float          m_heartbeatTimer  = 0.f;
    bool           m_rendererReady   = false;
    float          m_zoomDistance     = 1.0f;
    XMFLOAT3       m_smoothedEye     = {0.f, 80.f, -50.f};
    XMFLOAT3       m_smoothedLookAt  = {0.f, 0.f, 15.f};

    inline static std::atomic<int> sm_scrollAccum = 0;

    Canvas      m_canvas;
    BitmapFont  m_editorFont;
    BitmapFont  m_monoFont;
    std::unique_ptr<GuiWindow> m_debugWindow;
    std::unique_ptr<GuiWindow> m_objectsWindow;
  };
}
