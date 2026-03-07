#pragma once

#include "ASyncLoader.h"
#include "DeviceNotify.h"
#include "TimerCore.h"

namespace Neuron
{
  class GameMain : public implements<GameMain, Windows::Foundation::IInspectable>, public IDeviceNotify, protected ASyncLoader
  {
    public:
      GameMain() = default;
      ~GameMain() override = default;

      // This function can be used to initialize application state and will run after essential
      // hardware resources are allocated.  Some state that does not depend on these resources
      // should still be initialized in the constructor such as pointers and flags.
      virtual void Startup() = 0;
      virtual void Shutdown() = 0;

      virtual void CreateDeviceDependentResources() {}
      virtual void CreateWindowSizeDependentResources() {}
      virtual void ReleaseDeviceDependentResources() {}
      virtual void ReleaseWindowSizeDependentResources() {}

      // The update method will be invoked once per frame.  Both state updating and scene
      // rendering should be handled by this method.
      // 
      virtual void Update(float _deltaT) = 0;

      // Official rendering pass
      virtual void RenderScene() = 0;

      // Optional UI (overlay) rendering pass.  This is LDR.  The buffer is already cleared.
      virtual void RenderCanvas() {}

      virtual void AddTouch([[maybe_unused]] int _id, [[maybe_unused]] Windows::Foundation::Point _point) {}
      virtual void UpdateTouch([[maybe_unused]] int _id, [[maybe_unused]] Windows::Foundation::Point _point) {}
      virtual void RemoveTouch([[maybe_unused]] int _id) {}

      virtual void OnActivated() {}

      virtual void OnDeactivated() {}

      virtual void OnSuspending() {}

      virtual void OnResuming() { Timer::Core::ResetElapsedTime(); }

      virtual void OnWindowMoved()
      {
        const auto r = Graphics::Core::GetOutputSize();
        Graphics::Core::WindowSizeChanged(r.right - r.left, r.bottom - r.top);
      }

      virtual void OnDisplayChange() { Graphics::Core::UpdateColorSpace(); }

      virtual void OnWindowSizeChanged(int width, int height) { Graphics::Core::WindowSizeChanged(width, height); }

      // IDeviceNotify
      void OnDeviceLost() override {}

      void OnDeviceRestored() override {}
  };
}
