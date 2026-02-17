#include "pch.h"
#include "Camera.h"

Camera::Camera()
{
  // Setup the view matrix.
  SetViewParams(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

  // Setup the projection matrix.
  SetProjParams(XM_PI / 4, 1.0f, 1.0f, 1000.0f);
}

void XM_CALLCONV Camera::LookDirection(FXMVECTOR _lookDirection)
{
  XMVECTOR lookAt = XMVectorAdd(XMLoadFloat3(&m_eye), _lookDirection);

  SetViewParams(XMLoadFloat3(&m_eye), lookAt, XMLoadFloat3(&m_up));
}

void XM_CALLCONV Camera::Eye(FXMVECTOR _position) { SetViewParams(_position, XMLoadFloat3(&m_lookAt), XMLoadFloat3(&m_up)); }

void XM_CALLCONV Camera::SetViewParams(FXMVECTOR _eye, FXMVECTOR _lookAt, FXMVECTOR _up)
{
  XMStoreFloat3(&m_eye, _eye);
  XMStoreFloat3(&m_lookAt, _lookAt);
  XMStoreFloat3(&m_up, _up);

  // Calculate the view matrix.
  m_viewMatrix = XMMatrixLookAtLH(_eye, _lookAt, _up);

  XMVECTOR det;
  XMMATRIX inverseView = XMMatrixInverse(&det, m_viewMatrix);
  m_inverseView = inverseView;

  // The axis basis vectors and camera position are stored inside the
  // position matrix in the 4 rows of the camera's world matrix.
  // To figure out the yaw/pitch of the camera, we just need the Z basis vector.
  XMFLOAT3 zBasis;
  XMStoreFloat3(&zBasis, inverseView.r[2]);

  m_cameraYawAngle = atan2f(zBasis.x, zBasis.z);

  float len = sqrtf(zBasis.z * zBasis.z + zBasis.x * zBasis.x);
  m_cameraPitchAngle = atan2f(zBasis.y, len);
  m_viewProj = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);
}

void Camera::SetProjParams(_In_ float fieldOfView, _In_ float aspectRatio, _In_ float nearPlane, _In_ float farPlane)
{
  // Set attributes for the projection matrix.
  m_fieldOfView = fieldOfView;
  m_aspectRatio = aspectRatio;
  m_nearPlane = nearPlane;
  m_farPlane = farPlane;
  m_projectionMatrix = XMMatrixPerspectiveFovLH(m_fieldOfView, m_aspectRatio, m_nearPlane, m_farPlane);
  m_viewProj = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);
}

XMFLOAT3 Camera::Eye() const { return m_eye; }

XMFLOAT3 Camera::LookAt() const { return m_lookAt; }

XMFLOAT3 Camera::Up() const { return m_up; }

void Camera::RotateYawPitch(float deltaYaw, float deltaPitch)
{
  // Update yaw and pitch angles
  m_cameraYawAngle += deltaYaw;
  m_cameraPitchAngle += deltaPitch;

  // Clamp pitch to avoid flipping
  constexpr float limit = XM_PIDIV2 - 0.01f;
  m_cameraPitchAngle = std::clamp(m_cameraPitchAngle, -limit, limit);

  // Calculate new look direction
  float cosPitch = cosf(m_cameraPitchAngle);
  XMFLOAT3 lookDir;
  lookDir.x = sinf(m_cameraYawAngle) * cosPitch;
  lookDir.y = sinf(m_cameraPitchAngle);
  lookDir.z = cosf(m_cameraYawAngle) * cosPitch;

  // Update lookAt position
  XMFLOAT3 newLookAt;
  newLookAt.x = m_eye.x + lookDir.x;
  newLookAt.y = m_eye.y + lookDir.y;
  newLookAt.z = m_eye.z + lookDir.z;

  SetViewParams(XMLoadFloat3(&m_eye), XMLoadFloat3(&newLookAt), XMLoadFloat3(&m_up));
}
