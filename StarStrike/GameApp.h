#pragma once

#include "Camera.h"
#include "ClientSocket.h"
#include "EntityCache.h"
#include "InputSystem.h"

class GameApp : public GameMain
{
public:
  GameApp();
  ~GameApp() override;

  void Startup() override;
  void Shutdown() override;

  void CreateDeviceDependentResources() override;
  void CreateWindowSizeDependentResources() override;
  void ReleaseDeviceDependentResources() override;
  void ReleaseWindowSizeDependentResources() override;

  void Update([[maybe_unused]] float _deltaT) override;
  void RenderScene() override;
  void RenderCanvas() override;

  void OnDeviceRestored() override;
  void OnWindowSizeChanged(int width, int height) override;

  // Touch input for camera panning
  void AddTouch(int _id, Windows::Foundation::Point _point) override;
  void UpdateTouch(int _id, Windows::Foundation::Point _point) override;
  void RemoveTouch(int _id) override;

protected:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  std::string m_isoLang;

  // Track active touch ID for single-finger panning
  int m_activeTouchId = -1;

  // ── Phase 4 subsystems ─────────────────────────────────────────────────
  Neuron::Client::ClientSocket m_socket;
  Neuron::Client::EntityCache  m_entityCache;
  Neuron::Client::InputSystem  m_input;
  Neuron::Client::Camera       m_camera;

  Neuron::PlayerID m_localPlayerId = 0;
  uint64_t         m_localTick     = 0;

  // Server connection config (hardcoded for now; Phase 6 adds UI)
  static constexpr const char* DEFAULT_SERVER_ADDR = "127.0.0.1";
  static constexpr uint16_t    DEFAULT_SERVER_PORT = 7777;
};

extern GameApp* g_app;
