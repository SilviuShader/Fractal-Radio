#pragma once

#include <chrono>
#include <dxgi1_6.h>

class Graphics  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:

    Graphics(bool, bool, uint8_t);
    ~Graphics();

    void ToggleVSync();
    void Resize(uint32_t, uint32_t);

    void EndFrame();

    void BeginFrame()              const;
    void ClearRenderTarget(FLOAT*) const;

    bool IsInitialized()           const;

private:

           void                                              FreeBackBuffers();
           
           void                                              UpdateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                     Microsoft::WRL::ComPtr<IDXGISwapChain4>,
                                                                                     Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>);

           void                                              EnableDebugLayer()                                         const;
           bool                                              CheckTearingSupport()                                      const;

           Microsoft::WRL::ComPtr<IDXGIAdapter4>             GetAdapter(bool)                                           const;
           Microsoft::WRL::ComPtr<ID3D12Device2>             CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4>)        const;
           Microsoft::WRL::ComPtr<ID3D12CommandQueue>        CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2>, 
                                                                                D3D12_COMMAND_LIST_TYPE)                const;
           Microsoft::WRL::ComPtr<IDXGISwapChain4>           CreateSwapChain(HWND, Microsoft::WRL::ComPtr<ID3D12CommandQueue>,
                                                                             uint32_t, uint32_t, uint8_t)               const;

           Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                  D3D12_DESCRIPTOR_HEAP_TYPE, uint32_t) const;

           Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                    D3D12_COMMAND_LIST_TYPE)            const;

           Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                               Microsoft::WRL::ComPtr<ID3D12CommandAllocator>,
                                                                               D3D12_COMMAND_LIST_TYPE)                 const;

           Microsoft::WRL::ComPtr<ID3D12Fence>               CreateFence(Microsoft::WRL::ComPtr<ID3D12Device2>)         const;

    static uint64_t                                          Signal(Microsoft::WRL::ComPtr<ID3D12CommandQueue>, 
                                                                    Microsoft::WRL::ComPtr<ID3D12Fence>, uint64_t&);

    static void                                              WaitForFenceValue(Microsoft::WRL::ComPtr<ID3D12Fence>,
                                                                               uint64_t, HANDLE, 
                                                                               std::chrono::milliseconds = std::chrono::milliseconds::max());

    static void                                               Flush(Microsoft::WRL::ComPtr<ID3D12CommandQueue>, 
                                                                    Microsoft::WRL::ComPtr<ID3D12Fence>, 
                                                                    uint64_t&, HANDLE);

    static HANDLE                                             CreateFenceEvent();

    bool                                                      m_allowTearing;
    Microsoft::WRL::ComPtr<ID3D12Device2>                     m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>                m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>                   m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>              m_renderTargetViewDescriptorHeap;
    UINT                                                      m_numFrames;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>         m_commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence>                       m_fence;
    uint64_t                                                  m_fenceValue;
    uint64_t*                                                 m_frameFenceValues;
    HANDLE                                                    m_fenceEvent;
    bool                                                      m_isInitialized;
    bool                                                      m_vSync;

    Microsoft::WRL::ComPtr<ID3D12Resource>*                   m_backBuffers{};
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>*           m_commandAllocators{};
    UINT                                                      m_currentBackBufferIndex{};
    UINT                                                      m_renderTargetViewDescriptorSize{};
};