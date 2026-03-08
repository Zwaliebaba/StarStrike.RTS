#include "pch.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Neuron::Client
{

void Camera::init(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    m_zoom        = 1.0f;
    m_position    = { 0.0f, 0.0f, 0.0f };
}

void Camera::update(float panX, float panY, float zoomDelta, float dt)
{
    // Apply pan
    float speed = PAN_SPEED / m_zoom; // Faster pan when zoomed out
    m_position.x += panX * speed * dt;
    m_position.y += panY * speed * dt;

    // Apply zoom
    if (zoomDelta != 0.0f)
    {
        m_zoom *= (1.0f + zoomDelta * ZOOM_SPEED);
        m_zoom = std::clamp(m_zoom, MIN_ZOOM, MAX_ZOOM);
    }
}

void Camera::pan(float dx, float dy)
{
    m_position.x += dx;
    m_position.y += dy;
}

void Camera::setZoom(float zoom)
{
    m_zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

void Camera::lookAt(const Vec3& target)
{
    m_position = target;
}

XMMATRIX Camera::getViewMatrix() const
{
    // Convert angles to radians
    float yawRad   = XMConvertToRadians(m_yaw);
    float pitchRad = XMConvertToRadians(m_pitch);

    // Camera offset from look-at point (spherical coordinates)
    float cosP = cosf(pitchRad);
    float sinP = sinf(pitchRad);
    float cosY = cosf(yawRad);
    float sinY = sinf(yawRad);

    XMVECTOR eye = XMVectorSet(
        m_position.x + CAM_DISTANCE * cosP * sinY,
        m_position.y + CAM_DISTANCE * sinP,
        m_position.z + CAM_DISTANCE * cosP * cosY,
        1.0f);

    XMVECTOR target = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f);
    XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    return XMMatrixLookAtLH(eye, target, up);
}

XMMATRIX Camera::getProjectionMatrix() const
{
    // Orthographic projection for isometric view
    float halfHeight = ORTHO_SIZE / m_zoom;
    float halfWidth  = halfHeight * m_aspectRatio;

    return XMMatrixOrthographicLH(halfWidth * 2.0f, halfHeight * 2.0f, 0.1f, CAM_DISTANCE * 2.0f);
}

XMMATRIX Camera::getViewProjection() const
{
    return XMMatrixMultiply(getViewMatrix(), getProjectionMatrix());
}

} // namespace Neuron::Client
