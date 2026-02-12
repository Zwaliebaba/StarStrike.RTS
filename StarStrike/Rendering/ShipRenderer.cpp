#include "pch.h"
#include "ShipRenderer.h"
#include "GraphicsCore.h"
#include "FrameUploadAllocator.h"
#include "Color.h"
#include "shipdata.h"
#include "shipface.h"
#include "space.h"
#include "gfx.h"
#include "elite.h"

// Include compiled shader bytecode
#include "CompiledShaders/Ship3DVS.h"
#include "CompiledShaders/Ship3DPS.h"

using namespace Neuron::Graphics;

// External references to ship data
extern struct ship_data* ship_list[];
extern struct ship_solid ship_solids[];

namespace StarStrike
{
  void ShipRenderer::Startup()
  {
    if (sm_initialized)
      return;

    DebugTrace("ShipRenderer::Startup\n");

    CreateResources();
    CreatePipelineStates();

    // Initialize camera to identity
    sm_cameraMatrix = XMMatrixIdentity();

    sm_initialized = true;
    DebugTrace("ShipRenderer initialized\n");
  }

  void ShipRenderer::Shutdown()
  {
    if (!sm_initialized)
      return;

    DebugTrace("ShipRenderer::Shutdown\n");

    Core::WaitForGpu();

    sm_vertices.clear();
    sm_lineVertices.clear();

    sm_initialized = false;
  }

  void ShipRenderer::CreateResources()
  {
    // Create root signature: one CBV for matrices
    sm_rootSignature.Reset(1, 0);
    sm_rootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    sm_rootSignature.Finalize(L"Ship3D Root Signature",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);

    // Reserve space for vertex accumulation (actual GPU memory allocated per-frame from FrameUploadAllocator)
    sm_vertices.reserve(MAX_SHIP_VERTICES);
    sm_lineVertices.reserve(256);
  }

  void ShipRenderer::CreatePipelineStates()
  {
    // Input layout for ship vertices
    static const D3D12_INPUT_ELEMENT_DESC shipInputLayout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {shipInputLayout, _countof(shipInputLayout)};

    // Depth stencil for 3D rendering
    CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    // Rasterizer for solid rendering
    CD3DX12_RASTERIZER_DESC solidRasterizer(D3D12_DEFAULT);
    solidRasterizer.CullMode = D3D12_CULL_MODE_BACK;
    solidRasterizer.FrontCounterClockwise = TRUE;  // Match OpenGL winding

    // Solid PSO
    sm_solidPSO.SetRootSignature(sm_rootSignature);
    sm_solidPSO.SetVertexShader(g_pShip3DVS, sizeof(g_pShip3DVS));
    sm_solidPSO.SetPixelShader(g_pShip3DPS, sizeof(g_pShip3DPS));
    sm_solidPSO.SetInputLayout(&inputLayoutDesc);
    sm_solidPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_solidPSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), Core::GetDepthBufferFormat());
    sm_solidPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_solidPSO.SetRasterizerState(solidRasterizer);
    sm_solidPSO.SetDepthStencilState(depthDesc);
    sm_solidPSO.Finalize();

    // Wireframe PSO
    CD3DX12_RASTERIZER_DESC wireRasterizer(D3D12_DEFAULT);
    wireRasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    wireRasterizer.CullMode = D3D12_CULL_MODE_NONE;

    sm_wireframePSO.SetRootSignature(sm_rootSignature);
    sm_wireframePSO.SetVertexShader(g_pShip3DVS, sizeof(g_pShip3DVS));
    sm_wireframePSO.SetPixelShader(g_pShip3DPS, sizeof(g_pShip3DPS));
    sm_wireframePSO.SetInputLayout(&inputLayoutDesc);
    sm_wireframePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_wireframePSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), Core::GetDepthBufferFormat());
    sm_wireframePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_wireframePSO.SetRasterizerState(wireRasterizer);
    sm_wireframePSO.SetDepthStencilState(depthDesc);
    sm_wireframePSO.Finalize();

    // Line PSO for laser effects
    CD3DX12_DEPTH_STENCIL_DESC lineDepthDesc(D3D12_DEFAULT);
    lineDepthDesc.DepthEnable = FALSE;  // Lasers draw on top

    sm_linePSO.SetRootSignature(sm_rootSignature);
    sm_linePSO.SetVertexShader(g_pShip3DVS, sizeof(g_pShip3DVS));
    sm_linePSO.SetPixelShader(g_pShip3DPS, sizeof(g_pShip3DPS));
    sm_linePSO.SetInputLayout(&inputLayoutDesc);
    sm_linePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    sm_linePSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    sm_linePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_linePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_linePSO.SetDepthStencilState(lineDepthDesc);
    sm_linePSO.Finalize();

    sm_lineVertices.reserve(256);
  }

  void ShipRenderer::SetCamera(int angle)
  {
    // The view transformation is already handled by switch_to_view() in threed.cpp
    // which modifies the object coordinates based on the view direction.
    // So we just use identity for the camera/view matrix.
    sm_cameraMatrix = XMMatrixIdentity();
  }

  XMMATRIX ShipRenderer::BuildProjectionMatrix()
  {
    // Match the OpenGL frustum: glFrustum(-8, 8, -6, 6, 16, 65535)
    float left = -8.0f;
    float right = 8.0f;
    float bottom = -6.0f;
    float top = 6.0f;
    float nearZ = 16.0f;
    float farZ = 65535.0f;

    return XMMatrixPerspectiveOffCenterLH(left, right, bottom, top, nearZ, farZ);
  }

  XMMATRIX ShipRenderer::BuildViewMatrix()
  {
    return sm_cameraMatrix;
  }

  XMMATRIX ShipRenderer::BuildWorldMatrix(univ_object* univ)
  {
    // Build world matrix from object's rotation matrix and position
    // The switch_to_view function already handles view transformations,
    // so we use coordinates directly for left-handed DX12

    XMMATRIX world = XMMatrixIdentity();

    // Rotation part (3x3 from rotmat)
    world.r[0] = XMVectorSet(
      static_cast<float>(univ->rotmat[0].x),
      static_cast<float>(univ->rotmat[0].y),
      static_cast<float>(univ->rotmat[0].z),
      0.0f);
    world.r[1] = XMVectorSet(
      static_cast<float>(univ->rotmat[1].x),
      static_cast<float>(univ->rotmat[1].y),
      static_cast<float>(univ->rotmat[1].z),
      0.0f);
    world.r[2] = XMVectorSet(
      static_cast<float>(univ->rotmat[2].x),
      static_cast<float>(univ->rotmat[2].y),
      static_cast<float>(univ->rotmat[2].z),
      0.0f);

    // Translation part
    world.r[3] = XMVectorSet(
      static_cast<float>(univ->location.x),
      static_cast<float>(univ->location.y),
      static_cast<float>(univ->location.z),
      1.0f);

    return world;
  }

  XMFLOAT4 ShipRenderer::PaletteToColor(int paletteIndex)
  {
    XMFLOAT4 result;
    switch (paletteIndex)
    {
      case GFX_COL_BLACK:     XMStoreFloat4(&result, Color::BLACK); break;
      case GFX_COL_WHITE:     XMStoreFloat4(&result, Color::WHITE); break;
      case GFX_COL_WHITE_2:   XMStoreFloat4(&result, Color::WHITE_SMOKE); break;
      case GFX_COL_GOLD:      XMStoreFloat4(&result, Color::GOLD); break;
      case GFX_COL_RED:       XMStoreFloat4(&result, Color::RED); break;
      case GFX_COL_RED_3:     XMStoreFloat4(&result, Color::DARK_RED); break;
      case GFX_COL_RED_4:     XMStoreFloat4(&result, Color::FIREBRICK); break;
      case GFX_COL_DARK_RED:  XMStoreFloat4(&result, Color::DARK_RED); break;
      case GFX_COL_CYAN:      XMStoreFloat4(&result, Color::CYAN); break;
      case GFX_COL_GREY_1:    XMStoreFloat4(&result, Color::LIGHT_GRAY); break;
      case GFX_COL_GREY_2:    XMStoreFloat4(&result, Color::GRAY); break;
      case GFX_COL_GREY_3:    XMStoreFloat4(&result, Color::DIM_GRAY); break;
      case GFX_COL_GREY_4:    XMStoreFloat4(&result, Color::DARK_GRAY); break;
      case GFX_COL_BLUE_1:    XMStoreFloat4(&result, Color::BLUE); break;
      case GFX_COL_BLUE_2:    XMStoreFloat4(&result, Color::MEDIUM_BLUE); break;
      case GFX_COL_BLUE_3:    XMStoreFloat4(&result, Color::DARK_BLUE); break;
      case GFX_COL_BLUE_4:    XMStoreFloat4(&result, Color::NAVY); break;
      case GFX_COL_YELLOW_1:  XMStoreFloat4(&result, Color::YELLOW); break;
      // GFX_COL_YELLOW_2 (39) equals GFX_COL_GOLD, handled above
      case GFX_COL_YELLOW_3:  XMStoreFloat4(&result, Color::GOLDENROD); break;
      case GFX_COL_YELLOW_4:  XMStoreFloat4(&result, Color::DARK_GOLDENROD); break;
      case GFX_COL_YELLOW_5:  XMStoreFloat4(&result, Color::LIGHT_YELLOW); break;
      case GFX_ORANGE_1:      XMStoreFloat4(&result, Color::ORANGE); break;
      case GFX_ORANGE_2:      XMStoreFloat4(&result, Color::DARK_ORANGE); break;
      case GFX_ORANGE_3:      XMStoreFloat4(&result, Color::ORANGE_RED); break;
      case GFX_COL_GREEN_1:   XMStoreFloat4(&result, Color::GREEN); break;
      case GFX_COL_GREEN_2:   XMStoreFloat4(&result, Color::DARK_GREEN); break;
      case GFX_COL_GREEN_3:   XMStoreFloat4(&result, Color::LIME); break;
      case GFX_COL_PINK_1:    XMStoreFloat4(&result, Color::Pink); break;
      default:                XMStoreFloat4(&result, Color::WHITE); break;
    }
    return result;
  }

  void ShipRenderer::DrawFace(ship_face* face, ship_point* points, const XMFLOAT4& color)
  {
    int numPoints = face->points;

    if (numPoints < 3)
      return;  // Lines handled separately

    // Get point indices
    int indices[8] = {face->p1, face->p2, face->p3, face->p4, face->p5, face->p6, face->p7, face->p8};

    // Convert polygon to triangles (fan triangulation)
    for (int i = 1; i < numPoints - 1; i++)
    {
      ShipVertex v0, v1, v2;

      v0.Position = XMFLOAT3(
        static_cast<float>(points[indices[0]].x),
        static_cast<float>(points[indices[0]].y),
        static_cast<float>(points[indices[0]].z));
      v0.Color = color;

      v1.Position = XMFLOAT3(
        static_cast<float>(points[indices[i]].x),
        static_cast<float>(points[indices[i]].y),
        static_cast<float>(points[indices[i]].z));
      v1.Color = color;

      v2.Position = XMFLOAT3(
        static_cast<float>(points[indices[i + 1]].x),
        static_cast<float>(points[indices[i + 1]].y),
        static_cast<float>(points[indices[i + 1]].z));
      v2.Color = color;

      sm_vertices.push_back(v0);
      sm_vertices.push_back(v1);
      sm_vertices.push_back(v2);
    }
  }

  void ShipRenderer::DrawShip(univ_object* univ)
  {
    if (!sm_initialized || !univ)
      return;

    ship_data* ship = ship_list[univ->type];
    ship_solid* solid = &ship_solids[univ->type];

    if (!ship || !solid || !solid->face_data)
      return;

    // Build combined matrix
    XMMATRIX world = BuildWorldMatrix(univ);
    XMMATRIX view = BuildViewMatrix();
    XMMATRIX proj = BuildProjectionMatrix();
    XMMATRIX worldViewProj = world * view * proj;

    XMStoreFloat4x4(&sm_constants.WorldViewProj, XMMatrixTranspose(worldViewProj));

    // Clear vertices
    sm_vertices.clear();

    // Build geometry
    ship_face* faces = solid->face_data;
    for (int i = 0; i < solid->num_faces; i++)
    {
      XMFLOAT4 color = PaletteToColor(faces[i].colour);
      DrawFace(&faces[i], ship->points, color);
    }

    if (sm_vertices.empty())
      return;

    // Allocate per-frame constant buffer from ring buffer (256-byte aligned for CBV)
    auto cbAlloc = FrameUploadAllocator::Allocate(sizeof(Ship3DConstants), 256);
    memcpy(cbAlloc.cpuAddress, &sm_constants, sizeof(Ship3DConstants));

    // Allocate per-frame vertex buffer from ring buffer
    size_t vertexDataSize = sm_vertices.size() * sizeof(ShipVertex);
    auto vbAlloc = FrameUploadAllocator::Allocate(vertexDataSize, 16);
    memcpy(vbAlloc.cpuAddress, sm_vertices.data(), vertexDataSize);

    auto cmdList = Core::GetCommandList();

    // Set viewport and scissor rect for 3D rendering
    cmdList->RSSetViewports(1, &Core::GetScreenViewport());
    cmdList->RSSetScissorRects(1, &Core::GetScissorRect());

    // Set render target with depth
    auto rtvHandle = Core::GetRenderTargetView();
    auto dsvHandle = Core::GetDepthStencilView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Set pipeline state
    if (sm_wireframe)
    {
      cmdList->SetPipelineState(sm_wireframePSO.GetPipelineStateObject());
    }
    else
    {
      cmdList->SetPipelineState(sm_solidPSO.GetPipelineStateObject());
    }

    cmdList->SetGraphicsRootSignature(sm_rootSignature.GetSignature());
    cmdList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

    // Set vertex buffer
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vbAlloc.gpuAddress;
    vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
    vbv.StrideInBytes = sizeof(ShipVertex);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    cmdList->DrawInstanced(static_cast<UINT>(sm_vertices.size()), 1, 0, 0);
  }

  XMVECTOR ShipRenderer::TransformShipPoint(univ_object* univ, ship_point* point)
  {
    // Transform ship point by object's world matrix and camera
    XMVECTOR pos = XMVectorSet(
      static_cast<float>(point->x),
      static_cast<float>(point->y),
      static_cast<float>(point->z),
      1.0f);

    XMMATRIX world = BuildWorldMatrix(univ);
    XMMATRIX view = BuildViewMatrix();

    pos = XMVector3Transform(pos, world);
    pos = XMVector3Transform(pos, view);

    return pos;
  }

  void ShipRenderer::Draw3DLine(const XMVECTOR& start, const XMVECTOR& end, const XMFLOAT4& color)
  {
    ShipVertex v0, v1;

    XMStoreFloat3(&v0.Position, start);
    v0.Color = color;

    XMStoreFloat3(&v1.Position, end);
    v1.Color = color;

    sm_lineVertices.push_back(v0);
    sm_lineVertices.push_back(v1);
  }

  void ShipRenderer::DrawLaser(univ_object* univ)
  {
    if (!sm_initialized || !univ)
      return;

    // Check if ship is firing
    if (!(univ->flags & FLG_FIRING))
      return;

    ship_data* ship = ship_list[univ->type];
    if (!ship)
      return;

    int laserPointIndex = ship->front_laser;

    // Determine laser color
    int colorIndex = (univ->type == SHIP_VIPER && !sm_wireframe) ? GFX_COL_CYAN : GFX_COL_WHITE;
    XMFLOAT4 color = PaletteToColor(colorIndex);

    // Transform laser origin point
    XMVECTOR laserOrigin = TransformShipPoint(univ, &ship->points[laserPointIndex]);

    // Calculate target point near camera (randomized like OpenGL version)
    float targetX = XMVectorGetX(laserOrigin) > 0 ? -8.0f : 8.0f;
    float targetY = (rand() % 256) / 16.0f - 10.0f;
    float targetZ = -16.0f;
    XMVECTOR laserTarget = XMVectorSet(targetX, targetY, targetZ, 1.0f);

    // Clear line vertices
    sm_lineVertices.clear();

    // Add line
    Draw3DLine(laserOrigin, laserTarget, color);

    if (sm_lineVertices.empty())
      return;

    // Set up projection matrix for lines (identity view since we're in view space)
    XMMATRIX proj = BuildProjectionMatrix();
    XMStoreFloat4x4(&sm_constants.WorldViewProj, XMMatrixTranspose(proj));

    // Allocate per-frame constant buffer from ring buffer (256-byte aligned for CBV)
    auto cbAlloc = FrameUploadAllocator::Allocate(sizeof(Ship3DConstants), 256);
    memcpy(cbAlloc.cpuAddress, &sm_constants, sizeof(Ship3DConstants));

    // Allocate per-frame vertex buffer from ring buffer
    size_t vertexDataSize = sm_lineVertices.size() * sizeof(ShipVertex);
    auto vbAlloc = FrameUploadAllocator::Allocate(vertexDataSize, 16);
    memcpy(vbAlloc.cpuAddress, sm_lineVertices.data(), vertexDataSize);

    auto cmdList = Core::GetCommandList();

    // Set viewport and scissor rect for line rendering
    cmdList->RSSetViewports(1, &Core::GetScreenViewport());
    cmdList->RSSetScissorRects(1, &Core::GetScissorRect());

    // Set render target (no depth needed for lasers drawn on top)
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set line pipeline state
    cmdList->SetPipelineState(sm_linePSO.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(sm_rootSignature.GetSignature());
    cmdList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

    // Set vertex buffer
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vbAlloc.gpuAddress;
    vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
    vbv.StrideInBytes = sizeof(ShipVertex);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    // Draw
    cmdList->DrawInstanced(static_cast<UINT>(sm_lineVertices.size()), 1, 0, 0);
  }
}
