#include "pch.h"

#if defined(max)
#undef max
#endif

#include "Window.h"
#include "Graphics.h"

using namespace std;
using namespace std::chrono;
using namespace Microsoft::WRL;
using namespace DX;

Graphics::Graphics(const bool useWarp, const bool vSync, const uint8_t numFrames) :
    m_numFrames(numFrames),
    m_fenceValue(0),
    m_frameFenceValues(nullptr),
    m_isInitialized(false),
    m_vSync(vSync)
{
    EnableDebugLayer();
    m_allowTearing = CheckTearingSupport();
    const auto dxgiAdapter= GetAdapter(useWarp);
    m_device = CreateDevice(dxgiAdapter);
    m_commandQueue = CreateCommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_swapChain = CreateSwapChain(Window::GetInstance()->GetHWnd(), m_commandQueue, Window::GetInstance()->GetClientWidth(), Window::GetInstance()->GetClientHeight(), m_numFrames);
    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_renderTargetViewDescriptorHeap = CreateDescriptorHeap(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numFrames);
    m_renderTargetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UpdateRenderTargetViews(m_device, m_swapChain, m_renderTargetViewDescriptorHeap);
    m_commandAllocators = new ComPtr<ID3D12CommandAllocator>[m_numFrames];
    for (auto i = 0; i < m_numFrames; i++)
        m_commandAllocators[i] = CreateCommandAllocator(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_commandList = CreateCommandList(m_device, m_commandAllocators[m_currentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_fence = CreateFence(m_device);
    m_fenceEvent = CreateFenceEvent();
    m_frameFenceValues = new uint64_t[numFrames];
    memset(m_frameFenceValues, 0, sizeof uint64_t * numFrames);

    m_isInitialized = true;
}

Graphics::~Graphics()
{
    Flush(m_commandQueue, m_fence, m_fenceValue, m_fenceEvent);

    if (m_frameFenceValues)
    {
        delete[] m_frameFenceValues;
        m_frameFenceValues = nullptr;
    }

    if (m_commandAllocators)
    {
        delete[] m_commandAllocators;
        m_commandAllocators = nullptr;
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
    Flush(m_commandQueue, m_fence, m_fenceValue, m_fenceEvent);

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
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
void Graphics::ClearRenderTarget(FLOAT* clearColor) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_renderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currentBackBufferIndex, m_renderTargetViewDescriptorSize);

    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
void Graphics::EndFrame()
{
    const auto backBuffer = m_backBuffers[m_currentBackBufferIndex];

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* const commandLists[] =
    {
        m_commandList.Get()
    };

    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    const UINT syncInterval = m_vSync ? 1 : 0;
    const UINT presentFlags = m_allowTearing && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));

    m_frameFenceValues[m_currentBackBufferIndex] = Signal(m_commandQueue, m_fence, m_fenceValue);

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    WaitForFenceValue(m_fence, m_frameFenceValues[m_currentBackBufferIndex], m_fenceEvent);
}

// Based on https://www.3dgep.com/learning-directx-12-1/#render
void Graphics::BeginFrame() const
{
    auto commandAllocator = m_commandAllocators[m_currentBackBufferIndex];
    const auto backBuffer = m_backBuffers[m_currentBackBufferIndex];

    commandAllocator->Reset();
    m_commandList->Reset(commandAllocator.Get(), nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    m_commandList->ResourceBarrier(1, &barrier);
}


bool Graphics::IsInitialized() const
{
    return m_isInitialized;
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

// Based on https://www.3dgep.com/learning-directx-12-1/#create-the-command-queue
ComPtr<ID3D12CommandQueue> Graphics::CreateCommandQueue(ComPtr<ID3D12Device2> device,
                                                        D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc;

    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

    return d3d12CommandQueue;
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

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-command-allocator
ComPtr<ID3D12CommandAllocator> Graphics::CreateCommandAllocator(ComPtr<ID3D12Device2> device,
                                                                const D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-command-list
ComPtr<ID3D12GraphicsCommandList> Graphics::CreateCommandList(ComPtr<ID3D12Device2> device,
                                                              const ComPtr<ID3D12CommandAllocator> commandAllocator,
                                                              const D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    ThrowIfFailed(commandList->Close());

    return commandList;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-fence
ComPtr<ID3D12Fence> Graphics::CreateFence(ComPtr<ID3D12Device2> device) const
{
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#signal-the-fence
uint64_t Graphics::Signal(ComPtr<ID3D12CommandQueue> commandQueue, const ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
{
    const uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#wait-for-fence-value
// ReSharper disable once CppParameterMayBeConst
void Graphics::WaitForFenceValue(ComPtr<ID3D12Fence> fence, const uint64_t fenceValue, HANDLE fenceEvent, const milliseconds duration)
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

// Based on https://www.3dgep.com/learning-directx-12-1/#flush-the-gpu
void Graphics::Flush(const ComPtr<ID3D12CommandQueue> commandQueue, const ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, const HANDLE fenceEvent)
{
    const auto fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-an-event
HANDLE Graphics::CreateFenceEvent()
{
    // ReSharper disable once CppLocalVariableMayBeConst
    auto fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent)
    {
        MessageBox(nullptr, L"Failed to create fence event.", L"Error", MB_OK);
        assert(false);
    }

    return fenceEvent;
}