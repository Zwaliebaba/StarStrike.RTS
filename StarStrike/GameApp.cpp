#include "pch.h"
#include "GameApp.h"
#include "ClientNet.h"
#include "WndProcManager.h"

namespace Neuron
{
  void GameApp::Startup()
  {
    SetupIsometricCamera();

    m_worldRenderer.Startup();
    m_skyBox.Startup(L"Textures/starbox_1024.dds");

    m_canvas.Startup();
    m_editorFont.Load(L"Fonts/EditorFont-ENG.dds", 16, 16);
    m_monoFont.Load(L"Fonts/SpeccyFont-ENG.dds", 16, 16);
    m_debugWindow = std::make_unique<GuiWindow>("PROFILER", 800, 30, 400, 200);
    m_objectsWindow = std::make_unique<GuiWindow>("OBJECTS", 800, 240, 400, 200);

    m_rendererReady = true;

    m_clientWorld.Startup();

    WndProcManager::AddWndProc(WndProc);

    // Start networking and connect to server
    ClientNet::Startup();
    ClientNet::SetSnapshotCallback([this](const Net::WorldSnapshotPacket &_snapshot) { m_clientWorld.OnSnapshotReceived(_snapshot); });

    // Connect to localhost
    ClientNet::Connect("127.0.0.1", Net::DEFAULT_PORT);
    m_connected = false;
    m_heartbeatTimer = 0.f;

    DebugTrace("GameApp started\n");
  }

  void GameApp::Shutdown()
  {
    ClientNet::Disconnect();
    ClientNet::Shutdown();
    WndProcManager::RemoveWndProc(WndProc);
    m_clientWorld.Shutdown();
    m_debugWindow.reset();
    m_objectsWindow.reset();
    m_canvas.Shutdown();
    m_skyBox.Shutdown();
    m_worldRenderer.Shutdown();
    m_rendererReady = false;
    DebugTrace("GameApp shutdown\n");
  }

  LRESULT CALLBACK GameApp::WndProc(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam)
  {
    if (_message == WM_MOUSEWHEEL)
    {
      int delta = GET_WHEEL_DELTA_WPARAM(_wParam);
      sm_scrollAccum.fetch_add(delta, std::memory_order_relaxed);
      return 0;
    }
    return DefWindowProc(_hWnd, _message, _wParam, _lParam);
  }

  void GameApp::SetupIsometricCamera()
  {
    // Fixed isometric-style camera looking down at the world
    XMFLOAT3 eye = {0.f, 80.f, -50.f};
    XMFLOAT3 lookAt = {0.f, 0.f, 15.f};
    XMFLOAT3 up = {0.f, 1.f, 0.f};
    m_camera.SetViewParams(XMLoadFloat3(&eye), XMLoadFloat3(&lookAt), XMLoadFloat3(&up));

    auto outputSize = ClientEngine::OutputSize();
    float aspect = outputSize.Width / outputSize.Height;
    m_camera.SetProjParams(XM_PIDIV4, aspect, 1.0f, 5000.0f);
  }

  void GameApp::Update(float _deltaT)
  {
    // Poll network
    ClientNet::Poll();

    // Check connection state
    if (!m_connected && ClientNet::IsConnected())
    {
      m_connected = true;
      m_clientWorld.SetLocalPlayerId(ClientNet::GetPlayerObjectId());
      DebugTrace("Connected! clientId={} objectId={}\n", ClientNet::GetClientId(), ClientNet::GetPlayerObjectId());
    }

    // Handle input
    HandleInput();

    // Update client world (prediction + interpolation)
    m_clientWorld.Update(_deltaT);

    // Camera follows local player with smoothing
    if (m_connected)
    {
      auto localId = m_clientWorld.GetLocalPlayerId();
      const auto &objects = m_clientWorld.GetObjects();
      auto it = objects.find(localId);
      if (it != objects.end())
      {
        const auto &pos = it->second.position;
        XMFLOAT3 desiredEye = {pos.x, 80.f * m_zoomDistance, pos.z - 50.f * m_zoomDistance};
        XMFLOAT3 desiredLookAt = {pos.x, 0.f, pos.z + 15.f};

        constexpr float CAM_SMOOTH = 12.0f;
        float t = 1.0f - expf(-CAM_SMOOTH * _deltaT);

        XMVECTOR curEye = XMLoadFloat3(&m_smoothedEye);
        XMVECTOR curLookAt = XMLoadFloat3(&m_smoothedLookAt);
        XMVECTOR targetEye = XMLoadFloat3(&desiredEye);
        XMVECTOR targetLookAt = XMLoadFloat3(&desiredLookAt);

        XMVECTOR newEye = XMVectorLerp(curEye, targetEye, t);
        XMVECTOR newLookAt = XMVectorLerp(curLookAt, targetLookAt, t);

        XMStoreFloat3(&m_smoothedEye, newEye);
        XMStoreFloat3(&m_smoothedLookAt, newLookAt);

        XMFLOAT3 up = {0.f, 1.f, 0.f};
        m_camera.SetViewParams(newEye, newLookAt, XMLoadFloat3(&up));
      }
    }

    // Heartbeat
    m_heartbeatTimer += _deltaT;
    if (m_heartbeatTimer > 2.0f)
    {
      ClientNet::SendHeartbeat();
      m_heartbeatTimer = 0.f;
    }
  }

  void GameApp::HandleInput()
  {
    // Mouse wheel zoom
    int scroll = sm_scrollAccum.exchange(0, std::memory_order_relaxed);
    if (scroll != 0)
    {
      constexpr float ZOOM_SPEED = 0.1f;
      constexpr float ZOOM_MIN = 0.2f;
      constexpr float ZOOM_MAX = 3.0f;
      m_zoomDistance -= static_cast<float>(scroll) / WHEEL_DELTA * ZOOM_SPEED;
      m_zoomDistance = std::clamp(m_zoomDistance, ZOOM_MIN, ZOOM_MAX);
    }

    if (!m_connected) return;

    // Right-click to move
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
    {
      POINT cursor;
      GetCursorPos(&cursor);
      ScreenToClient(ClientEngine::Window(), &cursor);

      XMFLOAT3 worldPos = ScreenToWorld(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
      m_clientWorld.IssueMoveTo(worldPos);
    }

    // S key to stop
    if (GetAsyncKeyState('S') & 0x0001) m_clientWorld.IssueStop();
  }

  XMFLOAT3 GameApp::ScreenToWorld(float _screenX, float _screenY) const
  {
    auto outputSize = ClientEngine::OutputSize();
    float width = outputSize.Width;
    float height = outputSize.Height;

    // Normalized device coordinates
    float ndcX = (2.0f * _screenX / width) - 1.0f;
    float ndcY = 1.0f - (2.0f * _screenY / height);

    XMMATRIX viewProj = m_camera.ViewProj();
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

    // Near and far points
    XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProj);
    XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProj);

    // Ray-plane intersection with Y=0 plane
    XMVECTOR dir = XMVectorSubtract(farPoint, nearPoint);
    float nearY = XMVectorGetY(nearPoint);
    float dirY = XMVectorGetY(dir);

    if (fabsf(dirY) < 0.0001f) return {0.f, 0.f, 0.f};

    float t = -nearY / dirY;

    XMVECTOR hitPoint = XMVectorAdd(nearPoint, XMVectorScale(dir, t));
    XMFLOAT3 result;
    XMStoreFloat3(&result, hitPoint);
    result.y = 0.f;
    return result;
  }

  void GameApp::Render()
  {
    if (!m_rendererReady) return;

    auto *cmdList = Graphics::Core::GetCommandList();

    auto rtvHandle = Graphics::Core::GetRenderTargetView();
    auto dsvHandle = Graphics::Core::GetDepthStencilView();

    // Clear - deep space black with slight blue tint
    constexpr float clearColor[] = {0.02f, 0.02f, 0.06f, 1.0f};
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    auto viewport = Graphics::Core::GetScreenViewport();
    auto scissor = Graphics::Core::GetScissorRect();
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    // Render skybox first (writes at depth=1.0, all world geometry renders in front)
    m_skyBox.Render(m_camera.View(), m_camera.Projection());

    // Render world objects
    XMMATRIX viewProj = m_camera.ViewProj();
    m_worldRenderer.Render(m_clientWorld.GetObjects(), m_clientWorld.GetLocalPlayerId(), viewProj);

    // --- 2D Canvas Overlay ---
    m_canvas.Begin();
    m_debugWindow->BeginWindow(m_canvas, m_monoFont);
    float elapsedSec = Timer::Core::GetElapsedSeconds();
    float deltaMs = elapsedSec * 1000.0f;
    int fps = (elapsedSec > 0.0f) ? static_cast<int>(1.0f / elapsedSec) : 0;
    m_debugWindow->TextLine(std::format("{:.2f} ms ({} fps)", deltaMs, fps));
    m_debugWindow->EndWindow();

    static constexpr const char* OBJECT_TYPE_NAMES[] = {
      "Ship", "Asteroid", "Crate", "JumpGate", "Projectile", "Station", "Turret"
    };
    static_assert(std::size(OBJECT_TYPE_NAMES) == static_cast<size_t>(SpaceObjectType::Count));

    const auto& stats = m_worldRenderer.GetRenderStats();
    m_objectsWindow->BeginWindow(m_canvas, m_monoFont);
    uint32_t totalObjects = 0;
    for (size_t i = 0; i < static_cast<size_t>(SpaceObjectType::Count); i++)
    {
      if (stats.counts[i] > 0)
      {
        m_objectsWindow->LabelRow(OBJECT_TYPE_NAMES[i], std::to_string(stats.counts[i]));
        totalObjects += stats.counts[i];
      }
    }
    m_objectsWindow->Separator();
    m_objectsWindow->LabelRow("Total", std::to_string(totalObjects));
    m_objectsWindow->LabelRow("Draw calls", std::to_string(stats.totalDrawCalls));
    m_objectsWindow->EndWindow();

    m_canvas.End();
  }
}