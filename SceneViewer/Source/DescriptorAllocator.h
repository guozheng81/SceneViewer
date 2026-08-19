#pragma once

#include "Utils.h"

#include <vector>
#include <queue>
#include <mutex>
#include <cassert>

struct SDescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = { 0 };
};

class CDescriptorAllocator 
{
private:
    struct DeferredFreeItem 
    {
        uint32_t HeapIndex;
        UINT64 FenceValue;
    };

	bool bIsBlockAllocating = false;
	bool bIsShaderVisible = false;

public:
    void Init(ID3D12Device* Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptors, bool ShaderVisible);
    SDescriptorHandle Allocate();

	D3D12_GPU_DESCRIPTOR_HANDLE BeginBlockAllocation();
	void EndBlockAllocation() { bIsBlockAllocating = false; }

    void DeferredFree(D3D12_CPU_DESCRIPTOR_HANDLE InCpuHandle);
    void CleanUp(UINT64 CompletedFenceValue);

    inline ID3D12DescriptorHeap* GetHeap() const { return Heap.Get(); }
	inline uint32_t GetDescriptorSize() const { return DescriptorSize; }

private:
    ComPtr<ID3D12DescriptorHeap> Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapCpuStart = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE HeapGpuStart = { 0 };

    uint32_t DescriptorSize = 0;
    uint32_t NumDescriptors_ = 0;
    uint32_t NextFreeIndex = 0;

    std::vector<uint32_t> FreeList;
    std::queue<DeferredFreeItem> DeferredQueue; // Queue maintains strict temporal FIFO order
    std::mutex Mutex;
};

