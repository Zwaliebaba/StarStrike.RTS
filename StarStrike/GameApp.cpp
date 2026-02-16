#include "pch.h"
#include "GameApp.h"
#include "ClientNet.h"

namespace Neuron
{
  void GameApp::Startup()
  {
    SetupIsometricCamera();

    m_worldRenderer.Startup();
    m_rendererReady = true;

    m_clientWorld.Startup();

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
    m_clientWorld.Shutdown();
    m_worldRenderer.Shutdown();
    m_rendererReady = false;
    DebugTrace("GameApp shutdown\n");
  }

  void GameApp::SetupIsometricCamera()
  {
    // Fixed isometric-style camera looking down at the world
    XMFLOAT3 eye = {0.f, 300.f, -200.f};
    XMFLOAT3 lookAt = {0.f, 0.f, 50.f};
    XMFLOAT3 up = {0.f, 1.f, 0.f};
    m_camera.SetViewParams(eye, lookAt, up);

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

    // Camera follows local player
    if (m_connected)
    {
      auto localId = m_clientWorld.GetLocalPlayerId();
      const auto &objects = m_clientWorld.GetObjects();
      auto it = objects.find(localId);
      if (it != objects.end())
      {
        const auto &pos = it->second.position;
        XMFLOAT3 eye = {pos.x, 300.f, pos.z - 200.f};
        XMFLOAT3 lookAt = {pos.x, 0.f, pos.z + 50.f};
        XMFLOAT3 up = {0.f, 1.f, 0.f};
        m_camera.SetViewParams(eye, lookAt, up);
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

    // Render world objects
    XMMATRIX viewProj = m_camera.ViewProj();
    m_worldRenderer.Render(m_clientWorld.GetObjects(), m_clientWorld.GetLocalPlayerId(), viewProj);
  }
}