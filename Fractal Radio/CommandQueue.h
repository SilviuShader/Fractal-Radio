#pragma once

#include <queue>

// Class Heavily influenced by https://www.3dgep.com/learning-directx-12-2/#The_Command_Queue_Class
class CommandQueue
{
    struct AllocatorData
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
        uint64_t                                       FenceValue{};
    };

public:

    CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2>, D3D12_COMMAND_LIST_TYPE);

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

    uint64_t                                           ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>);
                                                       
    uint64_t                                           Signal();
    void                                               Flush();
    void                                               WaitForFenceValue(uint64_t) const;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>         GetCommandQueue() const;

private:

           Microsoft::WRL::ComPtr<ID3D12CommandQueue>              CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                      D3D12_COMMAND_LIST_TYPE)        const;
                                                                   
                                                                   
           Microsoft::WRL::ComPtr<ID3D12Fence>                     CreateFence(Microsoft::WRL::ComPtr<ID3D12Device2>) const;
                                                                   
           Microsoft::WRL::ComPtr<ID3D12CommandAllocator>          CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                          D3D12_COMMAND_LIST_TYPE)    const;
                                                                   
           Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>      CreateCommandList(Microsoft::WRL::ComPtr<ID3D12Device2>,
                                                                                     Microsoft::WRL::ComPtr<ID3D12CommandAllocator>,
                                                                                     D3D12_COMMAND_LIST_TYPE)         const;
                                                                   
    static HANDLE                                                  CreateFenceEvent();

    Microsoft::WRL::ComPtr<ID3D12Device2>                          m_device;
    D3D12_COMMAND_LIST_TYPE                                        m_type;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>                     m_commandQueue;

    Microsoft::WRL::ComPtr<ID3D12Fence>                            m_fence;
    uint64_t                                                       m_fenceValue;
    HANDLE                                                         m_fenceEvent;

    std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_availableCommandLists{};
    std::queue<AllocatorData>                                      m_executingAllocators{};
};
