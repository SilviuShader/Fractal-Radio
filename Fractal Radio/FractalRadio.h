#pragma once
#include "Demo.h"

class FractalRadio final : public Demo
{
public:

    struct Vertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 Uv;
    };

    explicit FractalRadio(std::shared_ptr<Graphics>);

    void Update() override;
    void Render() override;

private:

    Microsoft::WRL::ComPtr<ID3D12Resource>      m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_drawRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_drawPipelineState;



    D3D12_VERTEX_BUFFER_VIEW                    m_vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW                     m_indexBufferView{};
};
