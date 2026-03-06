#pragma once

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

  // Touch input for camera panning
  void AddTouch(int _id, Windows::Foundation::Point _point) override;
  void UpdateTouch(int _id, Windows::Foundation::Point _point) override;
  void RemoveTouch(int _id) override;

protected:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  std::string m_isoLang;

  // Track active touch ID for single-finger panning
  int m_activeTouchId = -1;
};

extern GameApp* g_app;
