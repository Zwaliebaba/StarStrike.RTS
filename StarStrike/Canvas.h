#pragma once

class Canvas
{
public:
  static constexpr uint32_t WIDTH = 1920;
  static constexpr uint32_t HEIGHT = 1080;

  Canvas() = default;
  ~Canvas() = default;

  void CreateDeviceDependentResources();
  void ReleaseDeviceDependentResources();

  void Update(float _deltaT);
  void Render();

  // Accessors
  [[nodiscard]] ID3D12Resource *GetResource() { return m_renderTarget.GetResource(); }
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtvHandle; }
  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
  [[nodiscard]] uint32_t GetWidth() const { return WIDTH; }
  [[nodiscard]] uint32_t GetHeight() const { return HEIGHT; }
  [[nodiscard]] bool IsValid() const { return m_renderTarget.GetResource() != nullptr; }

private:
  GpuResource m_renderTarget;
  Graphics::DescriptorHandle m_srvHandle;
  D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle = {};
};