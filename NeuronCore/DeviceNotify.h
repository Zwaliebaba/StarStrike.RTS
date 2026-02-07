#pragma once

namespace Neuron
{
  // Provides an interface for an application that owns Core to be notified of the device being lost or created.
  interface IDeviceNotify
  {
    virtual ~IDeviceNotify() = default;
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
  };
}
