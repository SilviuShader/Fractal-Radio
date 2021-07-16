#pragma once
#include "Demo.h"

class FractalRadio final : public Demo
{
    struct RayMarcherBuffer
    {
        DirectX::XMFLOAT2 WindowSize;
    };

public:

    struct Vertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 Uv;
    };

    explicit FractalRadio(std::shared_ptr<Graphics>);

    void Resize(uint32_t, uint32_t) override;

    void Update()                   override;
    void Render()                   override;

private:
    
    void                                         RenderFractal(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>);

    void                                         CreateRayMarcherPipeline(Microsoft::WRL::ComPtr<ID3D12Device2>);
    void                                         CreateRayMarcherTexture(Microsoft::WRL::ComPtr<ID3D12Device2>);
    void                                         CreateFullscreenQuadPipeline(Microsoft::WRL::ComPtr<ID3D12Device2>);

    static uint32_t                              GetComputerShaderGroupsCount(uint32_t, uint32_t);

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_indexBuffer;
                                                 
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_fractalsTexture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_fractalTextureDescriptorUavHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_fractalTextureDescriptorSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_fractalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_fractalPipelineState;

    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_drawRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_drawPipelineState;
                                                 
    D3D12_VERTEX_BUFFER_VIEW                     m_vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW                      m_indexBufferView{};
};
