#include "pch.h"
#include "SkyBox.h"
#include "GraphicsCommon.h"
#include "TextureManager.h"
#include "CompiledShaders/SkyBoxVS.h"
#include "CompiledShaders/SkyBoxPS.h"

namespace Neuron
{
  void SkyBox::Startup(const std::wstring& _texturePath)
  {
    m_texture = Graphics::TextureManager::LoadFromFile(_texturePath);
    ASSERT_TEXT(m_texture != nullptr, L"SkyBox: Failed to load texture\n");

    // Root signature: 16 root constants at b0 (ViewProj = 16 floats) + 1 SRV + 1 static sampler
    m_rootSig.Reset(2, 1);
    m_rootSig[0].InitAsConstants(0, 16, D3D12_SHADER_VISIBILITY_VERTEX);
    m_rootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1,
                                        D3D12_SHADER_VISIBILITY_PIXEL);
    m_rootSig.InitStaticSampler(0, Graphics::SamplerLinearClampDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_rootSig.Finalize(L"SkyBoxRootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    // PSO: depth test <= (to pass at z=1), no depth write, no culling (render inside of cube)
    D3D12_DEPTH_STENCIL_DESC depthLessEqual = Graphics::DepthStateReadOnly;
    depthLessEqual.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    m_pso = GraphicsPSO(L"SkyBoxPSO");
    m_pso.SetRootSignature(m_rootSig);
    m_pso.SetVertexShader(g_pSkyBoxVS, sizeof(g_pSkyBoxVS));
    m_pso.SetPixelShader(g_pSkyBoxPS, sizeof(g_pSkyBoxPS));
    m_pso.SetInputLayout(&VertexPosition::INPUT_LAYOUT);
    m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_pso.SetRenderTargetFormat(Graphics::Core::GetBackBufferFormat(),
                                Graphics::Core::GetDepthBufferFormat());
    m_pso.SetRasterizerState(Graphics::RasterizerTwoSided);
    m_pso.SetBlendState(Graphics::BlendDisable);
    m_pso.SetDepthStencilState(depthLessEqual);
    m_pso.Finalize();

    CreateCubeMesh();

    DebugTrace("SkyBox started\n");
  }

  void SkyBox::Shutdown()
  {
    m_vertexBuffer = nullptr;
    m_uploadBuffer = nullptr;
    m_texture = nullptr;
    DebugTrace("SkyBox shutdown\n");
  }

  void SkyBox::CreateCubeMesh()
  {
    // Unit cube centered at origin — we render the inside faces
    constexpr float s = 1.0f;

    VertexPosition vertices[] =
    {
      // +X face
      {XMFLOAT3( s, -s, -s)}, {XMFLOAT3( s,  s, -s)}, {XMFLOAT3( s,  s,  s)},
      {XMFLOAT3( s, -s, -s)}, {XMFLOAT3( s,  s,  s)}, {XMFLOAT3( s, -s,  s)},
      // -X face
      {XMFLOAT3(-s, -s,  s)}, {XMFLOAT3(-s,  s,  s)}, {XMFLOAT3(-s,  s, -s)},
      {XMFLOAT3(-s, -s,  s)}, {XMFLOAT3(-s,  s, -s)}, {XMFLOAT3(-s, -s, -s)},
      // +Y face
      {XMFLOAT3(-s,  s,  s)}, {XMFLOAT3( s,  s,  s)}, {XMFLOAT3( s,  s, -s)},
      {XMFLOAT3(-s,  s,  s)}, {XMFLOAT3( s,  s, -s)}, {XMFLOAT3(-s,  s, -s)},
      // -Y face
      {XMFLOAT3(-s, -s, -s)}, {XMFLOAT3( s, -s, -s)}, {XMFLOAT3( s, -s,  s)},
      {XMFLOAT3(-s, -s, -s)}, {XMFLOAT3( s, -s,  s)}, {XMFLOAT3(-s, -s,  s)},
      // +Z face
      {XMFLOAT3( s, -s,  s)}, {XMFLOAT3( s,  s,  s)}, {XMFLOAT3(-s,  s,  s)},
      {XMFLOAT3( s, -s,  s)}, {XMFLOAT3(-s,  s,  s)}, {XMFLOAT3(-s, -s,  s)},
      // -Z face
      {XMFLOAT3(-s, -s, -s)}, {XMFLOAT3(-s,  s, -s)}, {XMFLOAT3( s,  s, -s)},
      {XMFLOAT3(-s, -s, -s)}, {XMFLOAT3( s,  s, -s)}, {XMFLOAT3( s, -s, -s)},
    };

    m_vertexCount = _countof(vertices);
    const UINT bufferSize = m_vertexCount * sizeof(VertexPosition);
    auto* device = Graphics::Core::GetD3DDevice();

    // Default heap
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    check_hresult(device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_vertexBuffer.put())));

    // Upload heap
    auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    check_hresult(device->CreateCommittedResource(
      &uploadProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_uploadBuffer.put())));

    // Map and copy
    void* mapped = nullptr;
    check_hresult(m_uploadBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, vertices, bufferSize);
    m_uploadBuffer->Unmap(0, nullptr);

    // Copy upload → default
    auto* cmdList = Graphics::Core::GetCommandList();
    if (!Graphics::Core::IsCommandListOpen())
      Graphics::Core::ResetCommandAllocatorAndCommandlist();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->CopyResource(m_vertexBuffer.get(), m_uploadBuffer.get());
    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmdList->ResourceBarrier(1, &barrier2);

    Graphics::Core::ExecuteCommandList(true);
    Graphics::Core::WaitForGpu();
    Graphics::Core::ResetCommandAllocatorAndCommandlist();

    // Upload buffer no longer needed after GPU copy is confirmed complete
    m_uploadBuffer = nullptr;

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = bufferSize;
    m_vbView.StrideInBytes = sizeof(VertexPosition);
  }

  void XM_CALLCONV SkyBox::Render(FXMMATRIX _view, CXMMATRIX _projection)
  {
    if (!m_texture || !m_texture->IsValid())
      return;

    // Remove translation from view matrix so skybox stays centered on camera
    XMMATRIX viewNoTranslation = _view;
    viewNoTranslation.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    XMMATRIX viewProj = viewNoTranslation * _projection;
    XMFLOAT4X4 viewProjT;
    XMStoreFloat4x4(&viewProjT, XMMatrixTranspose(viewProj));

    auto* cmdList = Graphics::Core::GetCommandList();

    cmdList->SetPipelineState(m_pso.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature());
    cmdList->SetGraphicsRoot32BitConstants(0, 16, &viewProjT, 0);

    Graphics::DescriptorAllocator::SetDescriptorHeaps(cmdList);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = m_texture->GetSRV();
    cmdList->SetGraphicsRootDescriptorTable(1, srv);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    cmdList->DrawInstanced(m_vertexCount, 1, 0, 0);
  }
}
