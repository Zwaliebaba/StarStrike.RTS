#include "pch.h"
#include "SamplerManager.h"
#include "GraphicsCore.h"
#include "Hash.h"

using namespace std;
using namespace Neuron::Graphics;

namespace
{
  map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> s_SamplerCache;
}

D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor() const
{
  auto device = Core::GetD3DDevice();

  size_t hashValue = HashState(this);
  auto iter = s_SamplerCache.find(hashValue);
  if (iter != s_SamplerCache.end())
  {
    return iter->second;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE Handle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  device->CreateSampler(this, Handle);
  return Handle;
}

void SamplerDesc::CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE Handle) const
{
  auto device = Core::GetD3DDevice();

  ASSERT(Handle.ptr != 0 && Handle.ptr != static_cast<size_t>(-1));
  device->CreateSampler(this, Handle);
}
