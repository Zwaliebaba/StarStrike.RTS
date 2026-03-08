#pragma once

#include "Types.h"

#include <DirectXMath.h>
#include <cstdint>

namespace Neuron::Client
{

/// Isometric camera controller.
/// Fixed pitch/yaw for isometric view; supports pan (WASD / mouse drag)
/// and zoom (mouse wheel). Produces view + projection matrices for rendering.
class Camera
{
public:
    Camera() = default;

    /// Initialize the camera with default isometric angles and aspect ratio.
    void init(float aspectRatio);

    /// Per-frame update: apply pan from keyboard deltas and zoom from wheel delta.
    /// @param panX   horizontal pan input (-1..1 from A/D keys)
    /// @param panY   vertical pan input (-1..1 from W/S keys)
    /// @param zoomDelta  mouse wheel delta (positive = zoom in)
    /// @param dt     frame delta time in seconds
    void update(float panX, float panY, float zoomDelta, float dt);

    /// Directly pan the camera by a world-space offset.
    void pan(float dx, float dy);

    /// Set zoom level directly.
    void setZoom(float zoom);

    /// Focus the camera on a world-space position.
    void lookAt(const Vec3& target);

    /// Get the combined view * projection matrix (row-major, DirectXMath convention).
    [[nodiscard]] DirectX::XMMATRIX getViewProjection() const;

    /// Get the view matrix.
    [[nodiscard]] DirectX::XMMATRIX getViewMatrix() const;

    /// Get the projection matrix.
    [[nodiscard]] DirectX::XMMATRIX getProjectionMatrix() const;

    /// Camera world position.
    [[nodiscard]] Vec3 position() const noexcept { return m_position; }

    /// Current zoom level.
    [[nodiscard]] float zoom() const noexcept { return m_zoom; }

    /// Update aspect ratio (e.g. on window resize).
    void setAspectRatio(float ar) { m_aspectRatio = ar; }

private:
    Vec3  m_position    = { 0.0f, 0.0f, 0.0f };
    float m_yaw         = 45.0f;   // Fixed isometric yaw (degrees)
    float m_pitch       = 35.264f; // arctan(1/sqrt(2)) ≈ 35.264° — true isometric
    float m_zoom        = 1.0f;
    float m_aspectRatio = 16.0f / 9.0f;

    static constexpr float PAN_SPEED    = 200.0f;  // Units per second
    static constexpr float ZOOM_SPEED   = 0.15f;   // Per wheel notch
    static constexpr float MIN_ZOOM     = 0.1f;
    static constexpr float MAX_ZOOM     = 10.0f;
    static constexpr float ORTHO_SIZE   = 100.0f;  // Base ortho half-height at zoom=1
    static constexpr float CAM_DISTANCE = 500.0f;  // Distance from look-at point
};

} // namespace Neuron::Client
