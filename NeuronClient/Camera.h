#pragma once

// Camera:
// This class defines the position, orientation, and viewing frustum of a camera looking into
// a 3D world. It will generate both the View matrix and Projection matrix. It can also
// provide a pair of Projection matrices to be used when stereoscopic 3D is used.

class Camera
{
  public:
    Camera();
    Camera(const Camera&) = delete;
    void operator=(const Camera&) = delete;

    void XM_CALLCONV SetViewParams(FXMVECTOR _eye, FXMVECTOR _lookAt, FXMVECTOR _up);
    void SetProjParams(_In_ float fieldOfView, _In_ float aspectRatio, _In_ float nearPlane, _In_ float farPlane);

    void XM_CALLCONV LookDirection(FXMVECTOR _lookDirection);
    void XM_CALLCONV Eye(FXMVECTOR _position);
    void RotateYawPitch(float deltaYaw, float deltaPitch);

    XMMATRIX View() { return m_viewMatrix; }
    XMMATRIX Projection() { return m_projectionMatrix; }
    XMMATRIX ViewProj() const { return m_viewProj; }
    XMMATRIX World() { return m_inverseView; }
    XMFLOAT3 Eye();
    XMFLOAT3 LookAt();
    XMFLOAT3 Up();
    float NearClipPlane() { return m_nearPlane; }
    float FarClipPlane() { return m_farPlane; }
    float Pitch() { return m_cameraPitchAngle; }
    float Yaw() { return m_cameraYawAngle; }

  private:
    XMMATRIX m_viewMatrix;
    XMMATRIX m_projectionMatrix;
    XMMATRIX m_viewProj;

    XMMATRIX m_inverseView;
        
    XMFLOAT3 m_eye;
    XMFLOAT3 m_lookAt;
    XMFLOAT3 m_up;
    float m_cameraYawAngle;
    float m_cameraPitchAngle;

    float m_fieldOfView;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};
