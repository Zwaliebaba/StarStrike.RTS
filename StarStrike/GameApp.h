#pragma once

#include "Camera.h"
#include "Canvas.h"
#include "CanvasWindow.h"
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

    Camera         m_camera;
    ClientWorld    m_clientWorld;
    WorldRenderer  m_worldRenderer;
    SkyBox         m_skyBox;
    bool           m_connected       = false;
    float          m_heartbeatTimer  = 0.f;
    bool           m_rendererReady   = false;
    XMFLOAT3       m_smoothedEye     = {0.f, 300.f, -200.f};
    XMFLOAT3       m_smoothedLookAt  = {0.f, 0.f, 50.f};

    Canvas      m_canvas;
    BitmapFont  m_editorFont;
    BitmapFont  m_monoFont;
    std::unique_ptr<CanvasWindow> m_debugWindow;
  };
}
