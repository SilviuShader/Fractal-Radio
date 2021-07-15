#include "pch.h"

#if defined(max)
#undef max
#endif

#include "Window.h"
#include "Graphics.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DX;

Graphics::Graphics(const bool useWarp, const bool vSync, const uint8_t numFrames) :
    m_numFrames(numFrames),
    m_frameFenceValues(nullptr),
    m_isInitialized(false),
    m_vSync(vSync),
    m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, LONG_MAX, LONG_MAX)),
    m_scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
    EnableDebugLayer();
    m_allowTearing = CheckTearingSupport();

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, Window::GetInstance()->GetClientWidth(),
                                  Window::GetInstance()->GetClientHeight());

    const auto dxgiAdapter= GetAdapter(useWarp);

    m_device = CreateDevice(dxgiAdapter);

    m_commandQueue = make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);

    m_swapChain = CreateSwapChain(Window::GetInstance()->GetHWnd(), m_commandQueue->GetCommandQueue(),
                                  Window::GetInstance()->GetClientWidth(), Window::GetInstance()->GetClientHeight(),
                                  m_numFrames);

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_renderTargetViewDescriptorHeap = CreateDescriptorHeap(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numFrames);

    m_renderTargetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(m_device, m_swapChain, m_renderTargetViewDescriptorHeap);

    m_frameFenceValues = new uint64_t[numFrames];
    memset(m_frameFenceValues, 0, sizeof uint64_t * numFrames);
    
    m_isInitialized = true;
}

Graphics::~Graphics()
{
    m_commandQueue->Flush();

    if (m_frameFenceValues)
    {
        delete[] m_frameFenceValues;
        m_frameFenceValues = nullptr;
    }

    FreeBackBuffers();
}

void Graphics::ToggleVSync()
{
    m_vSync = !m_vSync;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#resize
void Graphics::Resize(const uint32_t width, const uint32_t height)
{
    m_commandQueue->Flush();
    
    for (int i = 0; i < m_numFrames; i++)
    {
        m_backBuffers[i].Reset();
        m_frameFenceValues[i] = m_frameFenceValues[m_currentBackBufferIndex];
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(m_swapChain->ResizeBuffers(m_numFrames, width, height,
        swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    UpdateRenderTargetViews(m_device, m_swapChain, m_renderTargetViewDescriptorHeap);

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height));
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
void Graphics::EndFrame(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    const auto backBuffer = m_backBuffers[m_currentBackBufferIndex];

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    commandList->ResourceBarrier(1, &barrier);

    const auto fenceValue = m_commandQueue->ExecuteCommandList(commandList);

    const UINT syncInterval = m_vSync ? 1 : 0;
    const UINT presentFlags = m_allowTearing && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));

    m_frameFenceValues[m_currentBackBufferIndex] = fenceValue;

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_commandQueue->WaitForFenceValue(m_frameFenceValues[m_currentBackBufferIndex]);
}

// Based on https://www.3dgep.com/learning-directx-12-2/#tutorial2updatebufferresource
void Graphics::UpdateBufferResource(const ComPtr<ID3D12GraphicsCommandList2> commandList,
    ID3D12Resource** destinationResource, ID3D12Resource** intermediateResource,
    const size_t numElements, const size_t elementSize, const void* bufferData,
    const D3D12_RESOURCE_FLAGS flags) const
{
    const size_t bufferSize = numElements * elementSize;

    auto destHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto destResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &destHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &destResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(destinationResource)));

    if (bufferData)
    {
        auto intermediateHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto intermediateResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &intermediateHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &intermediateResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(intermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData;
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
            *destinationResource, *intermediateResource,
            0, 0, 1, &subresourceData);
    }
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
ComPtr<ID3D12GraphicsCommandList2> Graphics::BeginFrame() const
{
    auto commandList = m_commandQueue->GetCommandList();
    const auto backBuffer = m_backBuffers[m_currentBackBufferIndex];
    
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    commandList->ResourceBarrier(1, &barrier);

    return commandList;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
void Graphics::ClearRenderTarget(ComPtr<ID3D12GraphicsCommandList2> commandList, FLOAT* clearColor) const
{
    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_renderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currentBackBufferIndex, m_renderTargetViewDescriptorSize);

    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void Graphics::SetRenderTarget(ComPtr<ID3D12GraphicsCommandList2> commandList) const
{
    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_renderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currentBackBufferIndex, m_renderTargetViewDescriptorSize);

    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
}

bool Graphics::IsInitialized() const
{
    return m_isInitialized;
}

shared_ptr<CommandQueue> Graphics::GetCommandQueue() const
{
    return m_commandQueue;
}

ComPtr<ID3D12Device2> Graphics::GetDevice() const
{
    return m_device;
}

void Graphics::FreeBackBuffers()
{
    if (m_backBuffers)
    {
        delete[] m_backBuffers;
        m_backBuffers = nullptr;
    }
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-the-render-target-views
void Graphics::UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain,
                                       ComPtr<ID3D12DescriptorHeap> renderTargetViewDescriptorHeap)
{
    FreeBackBuffers();
    m_backBuffers = new ComPtr<ID3D12Resource>[m_numFrames];

    const auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(renderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < m_numFrames; i++)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
        m_backBuffers[i] = backBuffer;

        rtvHandle.Offset(rtvDescriptorSize);
    }
}

// Based on https://www.3dgep.com/learning-directx-12-1/#enable-the-direct3d-12-debug-layer
void Graphics::EnableDebugLayer() const
{
#if(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

// Based on https://www.3dgep.com/learning-directx-12-1/#check-for-tearing-support
bool Graphics::CheckTearingSupport() const
{
    BOOL result = FALSE;

    ComPtr<IDXGIFactory4> factory4;

    if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
            if (FAILED(factory5->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &result, sizeof result)))
                result = FALSE;
    }

    return result == TRUE;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#query-directx-12-adapter
ComPtr<IDXGIAdapter4> Graphics::GetAdapter(const bool useWarp) const
{
    ComPtr<IDXGIFactory4> dxgiFactory;
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (useWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; i++)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);


            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    return dxgiAdapter4;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-the-directx-12-device
ComPtr<ID3D12Device2> Graphics::CreateDevice(const ComPtr<IDXGIAdapter4> adapter) const
{
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_SEVERITY severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_MESSAGE_ID denyIds[] =
        {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
        };

        D3D12_INFO_QUEUE_FILTER newFilter = {};

        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        newFilter.DenyList.NumIDs = _countof(denyIds);
        newFilter.DenyList.pIDList = denyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&newFilter));
    }
#endif

    return d3d12Device2;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-the-swap-chain
// ReSharper disable once CppParameterMayBeConst
ComPtr<IDXGISwapChain4> Graphics::CreateSwapChain(HWND hWnd, const ComPtr<ID3D12CommandQueue> commandQueue,
                                                  const uint32_t width, const uint32_t height, const uint8_t bufferCount) const
{
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;

    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    return dxgiSwapChain4;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-descriptor-heap
ComPtr<ID3D12DescriptorHeap> Graphics::CreateDescriptorHeap(ComPtr<ID3D12Device2> device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t numDescriptors) const
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}