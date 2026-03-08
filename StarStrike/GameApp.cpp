#include "pch.h"
#include "GameApp.h"
#include "SnapshotDecoder.h"
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

  // Initialize camera with window aspect ratio
  auto outputSize = ClientEngine::OutputSize();
  float aspect = (outputSize.Height > 0) ? (outputSize.Width / outputSize.Height) : (16.0f / 9.0f);
  m_camera.init(aspect);

  // Connect to server
  if (m_socket.connect(DEFAULT_SERVER_ADDR, DEFAULT_SERVER_PORT))
    DebugTrace("GameApp: connected to server {}:{}\n", DEFAULT_SERVER_ADDR, DEFAULT_SERVER_PORT);
  else
    DebugTrace("GameApp: failed to connect to server\n");

  CreateDeviceDependentResources();
  CreateWindowSizeDependentResources();
}

void GameApp::Shutdown()
{
  m_socket.disconnect();

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

  m_input.resetFrame();

  // ── 1. Receive snapshots from server (non-blocking) ─────────────────
  if (m_socket.isConnected())
  {
    for (int i = 0; i < 10; ++i) // drain up to 10 packets per frame
    {
      auto pkt = m_socket.recv();
      if (!pkt)
        break;

      auto snap = Neuron::Client::decodeSnapshot(pkt->header.type, pkt->payload);
      if (snap)
      {
        m_entityCache.updateFromSnapshot(
            snap->serverTick,
            snap->entities.data(),
            static_cast<uint16_t>(snap->entities.size()));

        DebugTrace("Snapshot tick={}, entities={}\n", snap->serverTick, snap->entities.size());
      }
    }
  }

  // ── 2. Harvest & send input commands ────────────────────────────────
  auto commands = m_input.getPendingCommands(m_localPlayerId);
  if (m_socket.isConnected())
  {
    for (const auto& cmd : commands)
      m_socket.sendCommand(cmd);
  }

  // ── 3. Update camera from keyboard / mouse wheel ────────────────────
  float panX = 0.0f, panY = 0.0f;
  if (m_input.isKeyDown('W') || m_input.isKeyDown(VK_UP))    panY += 1.0f;
  if (m_input.isKeyDown('S') || m_input.isKeyDown(VK_DOWN))  panY -= 1.0f;
  if (m_input.isKeyDown('D') || m_input.isKeyDown(VK_RIGHT)) panX += 1.0f;
  if (m_input.isKeyDown('A') || m_input.isKeyDown(VK_LEFT))  panX -= 1.0f;

  m_camera.update(panX, panY, m_input.wheelDelta(), _deltaT);

  // ── 4. Interpolate entity positions for smooth rendering ────────────
  m_entityCache.interpolate(1.0f); // snap to target for now

  ++m_localTick;

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

void GameApp::OnWindowSizeChanged(int width, int height)
{
  GameMain::OnWindowSizeChanged(width, height);
  if (height > 0)
    m_camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
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

  // Forward input messages to the InputSystem
  if (g_app && g_app->m_input.processMessage(_hWnd, _message, _wParam, _lParam))
  {
    // InputSystem consumed the message — still let other handlers see it
  }

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
