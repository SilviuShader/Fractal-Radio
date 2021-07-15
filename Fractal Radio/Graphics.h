#pragma once

#include <dxgi1_6.h>

#include "CommandQueue.h"

class Graphics  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:

    Graphics(bool, bool, uint8_t);
    ~Graphics();

    void                                               ToggleVSync();
    void                                               Resize(uint32_t, uint32_t);

    void                                               EndFrame(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>);

    void                                               UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>,
                                                                            ID3D12Resource**, ID3D12Resource**,
                                                                            size_t, size_t, const void*, 
                                                                            D3D12_RESOURCE_FLAGS = D3D12_RESOURCE_FLAG_NONE)         const;

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> BeginFrame()                                                                  const;
    void                                               ClearRenderTarget(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, FLOAT*) const;
    void                                               SetRenderTarget(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>)                                                             const;

    bool                                               IsInitialized()                                                               const;

    std::shared_ptr<CommandQueue>                      GetCommandQueue()                                                             const;
    Microsoft::WRL::ComPtr<ID3D12Device2>              GetDevice()                                                                   const;

private:

           void                                         FreeBackBuffers();
           
           void                                         UpdateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                Microsoft::WRL::ComPtr<IDXGISwapChain4>,
                                                                                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>);

           void                                         EnableDebugLayer()                                         const;
           bool                                         CheckTearingSupport()                                      const;

           Microsoft::WRL::ComPtr<IDXGIAdapter4>        GetAdapter(bool)                                           const;
           Microsoft::WRL::ComPtr<ID3D12Device2>        CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4>)        const;
           Microsoft::WRL::ComPtr<IDXGISwapChain4>      CreateSwapChain(HWND, Microsoft::WRL::ComPtr<ID3D12CommandQueue>,
                                                                              uint32_t, uint32_t, uint8_t)         const;

           Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                             D3D12_DESCRIPTOR_HEAP_TYPE, uint32_t) const;

    bool                                                m_allowTearing;
    Microsoft::WRL::ComPtr<ID3D12Device2>               m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>             m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_renderTargetViewDescriptorHeap;
    UINT                                                m_numFrames;
    uint64_t*                                           m_frameFenceValues;
    bool                                                m_isInitialized;
    bool                                                m_vSync;
    D3D12_VIEWPORT                                      m_viewport;
    D3D12_RECT                                          m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12Resource>*             m_backBuffers{};
    UINT                                                m_currentBackBufferIndex{};
    UINT                                                m_renderTargetViewDescriptorSize{};

    std::shared_ptr<CommandQueue>                       m_commandQueue;
};
