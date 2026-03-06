#include "pch.h"
#include "GameApp.h"
#include "WndProcManager.h"

GameApp* g_app = nullptr;

GameApp::GameApp()
{
  g_app = this;

  WndProcManager::AddWndProc(WndProc);

  // Determine default language if possible (WinRT globalization APIs require
  // package identity; fall back gracefully for unpackaged desktop builds).
  try
  {
    auto isoLang = Windows::System::UserProfile::GlobalizationPreferences::Languages().GetAt(0);
    auto lang = std::wstring(Windows::Globalization::Language(isoLang).AbbreviatedName());
    auto country = Windows::Globalization::GeographicRegion().DisplayName();
    m_isoLang = to_string(isoLang);
  }
  catch (const winrt::hresult_error&)
  {
    m_isoLang = "en";
  }
}

GameApp::~GameApp() { WndProcManager::RemoveWndProc(WndProc); }

void GameApp::Startup()
{
  RegisterTouchWindow(ClientEngine::Window(), 0);
  EnableMouseInPointer(TRUE);

  CreateDeviceDependentResources();
  CreateWindowSizeDependentResources();
}

void GameApp::Shutdown()
{
  UnregisterTouchWindow(ClientEngine::Window());
  EnableMouseInPointer(FALSE);

  ReleaseDeviceDependentResources();
  ReleaseWindowSizeDependentResources();
}

void GameApp::CreateDeviceDependentResources()
{
  GameMain::CreateDeviceDependentResources();
}

void GameApp::CreateWindowSizeDependentResources()
{
  GameMain::CreateWindowSizeDependentResources();
}

void GameApp::ReleaseDeviceDependentResources()
{
  GameMain::ReleaseDeviceDependentResources();
}

void GameApp::ReleaseWindowSizeDependentResources()
{
  GameMain::ReleaseWindowSizeDependentResources();
}

void GameApp::Update(float _deltaT)
{
  StartProfile(L"Update");
  EndProfile();
}

void GameApp::RenderScene() {}

void GameApp::RenderCanvas()
{
  GameMain::RenderCanvas();
}

void GameApp::OnDeviceRestored()
{
  GameMain::OnDeviceRestored();
  Timer::Core::ResetElapsedTime();
}

//------------------------------------------------------------------------------
// Touch Input - Horizontal Camera Panning
//------------------------------------------------------------------------------

void GameApp::AddTouch(int _id, Windows::Foundation::Point _point)
{
  // Only track first touch for panning
  if (m_activeTouchId < 0)
  {
    m_activeTouchId = _id;
  }
}

void GameApp::UpdateTouch(int _id, Windows::Foundation::Point _point)
{
  if (_id == m_activeTouchId)
  {
  }
}

void GameApp::RemoveTouch(int _id)
{
  if (_id == m_activeTouchId)
  {
    m_activeTouchId = -1;
  }
}

LRESULT GameApp::WndProc(const HWND _hWnd, const UINT _message, WPARAM _wParam, const LPARAM _lParam)
{
  static bool s_rightButtonDown = false;

  switch (_message)
  {
  case WM_RBUTTONDOWN:
    s_rightButtonDown = true;
    SetCapture(_hWnd);
    ShowCursor(FALSE);
    break;

  case WM_RBUTTONUP:
    s_rightButtonDown = false;
    ReleaseCapture();
    ShowCursor(TRUE);
    break;
  }
  return -1;
}
