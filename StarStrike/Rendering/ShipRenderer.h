#pragma once

#include "PipelineState.h"
#include "RootSignature.h"
#include "VertexTypes.h"

// Forward declarations
struct univ_object;
struct ship_data;
struct ship_solid;
struct ship_face;
struct ship_point;

namespace StarStrike
{
  // Constant buffer for 3D ship rendering (must be 256-byte aligned)
  __declspec(align(256)) struct Ship3DConstants
  {
    XMFLOAT4X4 WorldViewProj;
  };

  // 3D vertex for ship rendering
  struct ShipVertex
  {
    XMFLOAT3 Position;
    XMFLOAT4 Color;
  };

  // Manages DirectX 12 3D ship rendering
  class ShipRenderer
  {
  public:
    static void Startup();
    static void Shutdown();

    // Set camera angle (view direction)
    static void SetCamera(int angle);

    // Render a ship object
    static void DrawShip(univ_object* univ);

    // Draw laser firing effect
    static void DrawLaser(univ_object* univ);

    // Set wireframe mode
    static void SetWireframe(bool enabled) { sm_wireframe = enabled; }
    static bool IsWireframe() { return sm_wireframe; }

  private:
    static void CreateResources();
    static void CreatePipelineStates();

    // Convert palette index to XMFLOAT4
    static XMFLOAT4 PaletteToColor(int paletteIndex);

    // Build projection matrix (frustum)
    static XMMATRIX BuildProjectionMatrix();

    // Build view matrix from camera
    static XMMATRIX BuildViewMatrix();

    // Build world matrix from object
    static XMMATRIX BuildWorldMatrix(univ_object* univ);

    // Transform a ship point to view space
    static XMVECTOR TransformShipPoint(univ_object* univ, ship_point* point);

    // Draw a single face of a ship
    static void DrawFace(ship_face* face, ship_point* points, const XMFLOAT4& color);

    // Draw a 3D line in view space
    static void Draw3DLine(const XMVECTOR& start, const XMVECTOR& end, const XMFLOAT4& color);

    // Root signature and PSOs
    inline static RootSignature sm_rootSignature;
    inline static GraphicsPSO sm_solidPSO{L"Ship Solid PSO"};
    inline static GraphicsPSO sm_wireframePSO{L"Ship Wireframe PSO"};
    inline static GraphicsPSO sm_linePSO{L"Ship Line PSO"};

    // CPU-side vertex accumulation (GPU memory allocated per-frame from FrameUploadAllocator)
    inline static std::vector<ShipVertex> sm_vertices;
    inline static std::vector<ShipVertex> sm_lineVertices;

    // Constant buffer staging (copied to per-frame allocation each draw)
    inline static Ship3DConstants sm_constants;

    // Camera matrix (4x4 doubles in legacy code, we use XMMATRIX)
    inline static XMMATRIX sm_cameraMatrix = XMMatrixIdentity();

    // Rendering state
    inline static bool sm_wireframe = false;
    inline static bool sm_initialized = false;

    static constexpr size_t MAX_SHIP_VERTICES = 4096;
  };
}
