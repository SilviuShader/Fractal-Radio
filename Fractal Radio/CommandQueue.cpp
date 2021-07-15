#include "pch.h"

#include "CommandQueue.h"

using namespace Microsoft::WRL;
using namespace DX;

CommandQueue::CommandQueue(const ComPtr<ID3D12Device2> device, const D3D12_COMMAND_LIST_TYPE type) :
    m_device(device),
    m_type(type),
    m_fenceValue(0)
{
    m_commandQueue = CreateCommandQueue(m_device, m_type);
    m_fence = CreateFence(m_device);
    m_fenceEvent = CreateFenceEvent();
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList2> commandList;

    if (!m_executingAllocators.empty() && m_executingAllocators.front().FenceValue <= m_fence->GetCompletedValue())  // NOLINT(bugprone-branch-clone)
    {
        commandAllocator = m_executingAllocators.front().CommandAllocator;
        m_executingAllocators.pop();
        commandAllocator->Reset();
    }
    else
    {
        commandAllocator = CreateCommandAllocator(m_device, m_type);
    }

    if (!m_availableCommandLists.empty())  // NOLINT(bugprone-branch-clone)
    {
        commandList = m_availableCommandLists.front();
        m_availableCommandLists.pop();

        commandList->Reset(commandAllocator.Get(), nullptr);
    }
    else
    {
        commandList = CreateCommandList(m_device, commandAllocator, m_type);
    }

    ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

    return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    commandList->Close();

    ID3D12CommandAllocator* commandAllocator;
    UINT dataSize = sizeof commandAllocator;  // NOLINT(bugprone-sizeof-expression)
    ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

    ID3D12CommandList* commandLists[] =
    {
        commandList.Get()
    };

    m_commandQueue->ExecuteCommandLists(1, commandLists);
    const auto fenceValue = Signal();

    m_executingAllocators.push({ commandAllocator, fenceValue });
    m_availableCommandLists.push(commandList);

    commandAllocator->Release();

    return fenceValue;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#signal-the-fence
uint64_t CommandQueue::Signal()
{
    const uint64_t fenceValueForSignal = ++m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#wait-for-fence-value
// ReSharper disable once CppParameterMayBeConst
void CommandQueue::WaitForFenceValue(uint64_t fenceValue) const
{
    if (m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, DWORD_MAX);
    }
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const
{
    return m_commandQueue;
}

void CommandQueue::Flush()
{
    WaitForFenceValue(Signal());
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-the-command-queue
ComPtr<ID3D12CommandQueue> CommandQueue::CreateCommandQueue(ComPtr<ID3D12Device2> device,
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

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-fence
ComPtr<ID3D12Fence> CommandQueue::CreateFence(ComPtr<ID3D12Device2> device) const
{
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-command-allocator
ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator(ComPtr<ID3D12Device2> device,
                                                                    const D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-a-command-list
ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(ComPtr<ID3D12Device2> device,
                                                                   const ComPtr<ID3D12CommandAllocator> commandAllocator,
                                                                   const D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12GraphicsCommandList2> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    return commandList;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-an-event
HANDLE CommandQueue::CreateFenceEvent()
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
