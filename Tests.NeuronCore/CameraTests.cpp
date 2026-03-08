#include "pch.h"

#include "Camera.h"

#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{

TEST_CLASS(CameraTests)
{
public:
    TEST_METHOD(InitSetsDefaults)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        Assert::AreEqual(1.0f, cam.zoom());
        Assert::AreEqual(0.0f, cam.position().x);
        Assert::AreEqual(0.0f, cam.position().y);
        Assert::AreEqual(0.0f, cam.position().z);
    }

    TEST_METHOD(PanMovesPosition)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        cam.pan(5.0f, 10.0f);

        Assert::AreEqual(5.0f, cam.position().x);
        Assert::AreEqual(10.0f, cam.position().y);
    }

    TEST_METHOD(UpdateWithPanMovesOverTime)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        // Pan right (panX=1) for 1 second at zoom=1
        cam.update(1.0f, 0.0f, 0.0f, 1.0f);

        // Position.x should have moved by PAN_SPEED (200.0)
        Assert::IsTrue(cam.position().x > 0.0f);
    }

    TEST_METHOD(UpdateWithZoomChangesZoomLevel)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        float zoomBefore = cam.zoom();
        cam.update(0.0f, 0.0f, 1.0f, 1.0f); // zoom in

        Assert::IsTrue(cam.zoom() > zoomBefore);
    }

    TEST_METHOD(ZoomClampedToMinMax)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        // Zoom way in
        for (int i = 0; i < 100; ++i)
            cam.update(0.0f, 0.0f, 10.0f, 1.0f);

        Assert::IsTrue(cam.zoom() <= 10.0f);

        // Zoom way out
        for (int i = 0; i < 200; ++i)
            cam.update(0.0f, 0.0f, -10.0f, 1.0f);

        Assert::IsTrue(cam.zoom() >= 0.1f);
    }

    TEST_METHOD(SetZoomDirectly)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        cam.setZoom(5.0f);
        Assert::AreEqual(5.0f, cam.zoom());
    }

    TEST_METHOD(SetZoomClamps)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        cam.setZoom(0.001f); // below MIN_ZOOM
        Assert::IsTrue(cam.zoom() >= 0.1f);

        cam.setZoom(999.0f); // above MAX_ZOOM
        Assert::IsTrue(cam.zoom() <= 10.0f);
    }

    TEST_METHOD(LookAtSetsPosition)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        Neuron::Vec3 target = { 50.0f, 60.0f, 70.0f };
        cam.lookAt(target);

        Assert::AreEqual(50.0f, cam.position().x);
        Assert::AreEqual(60.0f, cam.position().y);
        Assert::AreEqual(70.0f, cam.position().z);
    }

    TEST_METHOD(ViewMatrixNotIdentity)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        auto view = cam.getViewMatrix();
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, view);

        // An isometric camera at distance 500 with non-zero angles
        // should not produce an identity matrix
        bool isIdentity = (mat._11 == 1.0f && mat._22 == 1.0f &&
                           mat._33 == 1.0f && mat._44 == 1.0f &&
                           mat._12 == 0.0f && mat._13 == 0.0f);
        Assert::IsFalse(isIdentity);
    }

    TEST_METHOD(ProjectionMatrixNotIdentity)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        auto proj = cam.getProjectionMatrix();
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, proj);

        bool isIdentity = (mat._11 == 1.0f && mat._22 == 1.0f &&
                           mat._33 == 1.0f && mat._44 == 1.0f);
        Assert::IsFalse(isIdentity);
    }

    TEST_METHOD(ViewProjectionCombinesCorrectly)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        auto vp   = cam.getViewProjection();
        auto view = cam.getViewMatrix();
        auto proj = cam.getProjectionMatrix();
        auto expected = DirectX::XMMatrixMultiply(view, proj);

        // Compare element by element
        DirectX::XMFLOAT4X4 vpMat, expMat;
        DirectX::XMStoreFloat4x4(&vpMat, vp);
        DirectX::XMStoreFloat4x4(&expMat, expected);

        Assert::AreEqual(expMat._11, vpMat._11, 0.0001f);
        Assert::AreEqual(expMat._22, vpMat._22, 0.0001f);
        Assert::AreEqual(expMat._33, vpMat._33, 0.0001f);
        Assert::AreEqual(expMat._44, vpMat._44, 0.0001f);
    }

    TEST_METHOD(SetAspectRatioAffectsProjection)
    {
        Neuron::Client::Camera cam;
        cam.init(16.0f / 9.0f);

        DirectX::XMFLOAT4X4 mat1;
        DirectX::XMStoreFloat4x4(&mat1, cam.getProjectionMatrix());

        cam.setAspectRatio(4.0f / 3.0f);

        DirectX::XMFLOAT4X4 mat2;
        DirectX::XMStoreFloat4x4(&mat2, cam.getProjectionMatrix());

        // The _11 element (horizontal scale) should differ
        Assert::AreNotEqual(mat1._11, mat2._11);
    }
};

} // namespace Tests
