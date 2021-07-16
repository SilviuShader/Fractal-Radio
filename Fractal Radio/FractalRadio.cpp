#include "pch.h"

#include "FractalRadio.h"
#include "d3dcompiler.h"
#include "Window.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DX;

static FractalRadio::Vertex g_vertices[] =
{
    {XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    {XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    {XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0, 0.0f) },
    {XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }
};

static WORD g_indices[] =
{
    0, 1, 2,
    0, 2, 3
};

FractalRadio::FractalRadio(const shared_ptr<Graphics> graphics) :
    Demo(graphics)
{
    const auto device = graphics->GetDevice();
    auto commandQueue = graphics->GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();
    
    ComPtr<ID3D12Resource> vertexIntermediateResource;
    m_graphics->UpdateBufferResource(commandList, &m_vertexBuffer, &vertexIntermediateResource, _countof(g_vertices),
        sizeof Vertex, g_vertices);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = sizeof g_vertices;
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);

    ComPtr<ID3D12Resource> indexIntermediateResource;
    m_graphics->UpdateBufferResource(commandList, &m_indexBuffer, &indexIntermediateResource, _countof(g_indices),
        sizeof WORD, g_indices);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = sizeof g_indices;

    CreateRayMarcherPipeline(device);
    CreateFullscreenQuadPipeline(device);

    commandQueue->ExecuteCommandList(commandList);
    commandQueue->Flush();
}

void FractalRadio::Resize(uint32_t width, uint32_t height)
{
    auto commandQueue = m_graphics->GetCommandQueue();
    commandQueue->Flush();
    CreateRayMarcherTexture(m_graphics->GetDevice());
}

void FractalRadio::Update()
{
}

void FractalRadio::Render()
{
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

    auto commandQueue = m_graphics->GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();

    PIXBeginEvent(commandList.Get(), (UINT64)0, L"FractalStart");

    RenderFractal(commandList);

    PIXBeginEvent(commandList.Get(), (UINT64)0, L"FractalEnd");

    commandQueue->ExecuteCommandList(commandList);
    commandQueue->Flush();

    commandList = m_graphics->BeginFrame();

    PIXBeginEvent(commandList.Get(), (UINT64)0, L"FrameStart");

    m_graphics->ClearRenderTarget(commandList, clearColor);

    commandList->SetPipelineState(m_drawPipelineState.Get());
    commandList->SetGraphicsRootSignature(m_drawRootSignature.Get());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_fractalsTexture.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    commandList->ResourceBarrier(1, &barrier);

    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        m_fractalTextureDescriptorSrvHeap.Get()
    };

    commandList->SetDescriptorHeaps(1, descriptorHeaps);
;
    commandList->SetGraphicsRootDescriptorTable(
        0,
        m_fractalTextureDescriptorSrvHeap->GetGPUDescriptorHandleForHeapStart());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    m_graphics->SetRenderTarget(commandList);

    commandList->DrawIndexedInstanced(_countof(g_indices), 1, 0, 0, 0);

    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_fractalsTexture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

    commandList->ResourceBarrier(1, &barrier2);

    PIXBeginEvent(commandList.Get(), (UINT64)1000, L"FrameEnd");

    m_graphics->EndFrame(commandList);
}

uint32_t FractalRadio::GetComputerShaderGroupsCount(const uint32_t size, const uint32_t numBlocks)
{
    return (size + numBlocks - 1) / numBlocks;
}

void FractalRadio::RenderFractal(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    commandList->SetPipelineState(m_fractalPipelineState.Get());
    commandList->SetComputeRootSignature(m_fractalRootSignature.Get());

    ID3D12DescriptorHeap* descriptorHeaps[] = 
    {
        m_fractalTextureDescriptorUavHeap.Get()
    };

    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_fractalsTexture.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->ResourceBarrier(1, &barrier);
    
    commandList->SetComputeRootDescriptorTable(
        0,
        m_fractalTextureDescriptorUavHeap->GetGPUDescriptorHandleForHeapStart());
    
    commandList->Dispatch(GetComputerShaderGroupsCount(Window::GetInstance()->GetClientWidth(), 8),
                          GetComputerShaderGroupsCount(Window::GetInstance()->GetClientHeight(), 8), 1);

    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_fractalsTexture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

    commandList->ResourceBarrier(1, &barrier2);
}

void FractalRadio::CreateRayMarcherPipeline(ComPtr<ID3D12Device2> device)
{
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"RayMarcher.cso", &computeShaderBlob));

    CD3DX12_DESCRIPTOR_RANGE1 textureUav(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0,
                                         D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
    rootParameters[0].InitAsDescriptorTable(1, &textureUav);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(
        1, rootParameters,
        0, nullptr
    );

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &rootSignatureBlob, &errorBlob);

    device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_fractalRootSignature));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS Cs;
    } pipelineStateStream;

    pipelineStateStream.PRootSignature = m_fractalRootSignature.Get();
    pipelineStateStream.Cs = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_fractalPipelineState)));

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = 1;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_fractalTextureDescriptorUavHeap));
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_fractalTextureDescriptorSrvHeap));

    CreateRayMarcherTexture(device);
}

void FractalRadio::CreateRayMarcherTexture(ComPtr<ID3D12Device2> device)
{
    auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, Window::GetInstance()->GetClientWidth(),
        Window::GetInstance()->GetClientHeight());

    textureDesc.MipLevels = 1;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&m_fractalsTexture));

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_graphics->GetDevice()->CreateUnorderedAccessView(m_fractalsTexture.Get(), nullptr, &uavDesc, m_fractalTextureDescriptorUavHeap->GetCPUDescriptorHandleForHeapStart());
    m_graphics->GetDevice()->CreateShaderResourceView(m_fractalsTexture.Get(), &srvDesc, m_fractalTextureDescriptorSrvHeap->GetCPUDescriptorHandleForHeapStart());
}

void FractalRadio::CreateFullscreenQuadPipeline(ComPtr<ID3D12Device2> device)
{
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof featureData)))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        
    CD3DX12_DESCRIPTOR_RANGE1 textureSrv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};

    rootParameters[0].InitAsDescriptorTable(1, &textureSrv, D3D12_SHADER_VISIBILITY_PIXEL);
    CD3DX12_STATIC_SAMPLER_DESC pointClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(1, rootParameters, 1, &pointClampSampler, rootSignatureFlags);

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
        featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_drawRootSignature)));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
        CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    pipelineStateStream.PRootSignature = m_drawRootSignature.Get();
    pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.RtvFormats = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc =
    {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_drawPipelineState)));
}
