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
        UINT Offset;
        UINT64 FenceValue;
    };

    struct SReservedBlock
    {
        UINT StartOffset = 0;
        UINT NumDescriptors = 0;
        UINT NextFreeIndex = 0; // Absolute index (StartOffset-relative allocation cursor)
    };

	bool bIsShaderVisible = false;
	int ActiveBlockIndex = -1;

public:
    void Init(ID3D12Device* Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT NumDescriptors, bool ShaderVisible);
    SDescriptorHandle Allocate();

	UINT ReserveBlock(UINT InStartOffset, UINT NumDescriptors);
	void ResetReservedBlock(UINT BlockIndex);

	D3D12_GPU_DESCRIPTOR_HANDLE BeginBlockAllocation(UINT BlockIndex);
	void EndBlockAllocation() { ActiveBlockIndex = -1; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetReservedBlockGpuHandle(UINT BlockIndex) const;

    void DeferredFree(D3D12_CPU_DESCRIPTOR_HANDLE InCpuHandle);
    void CleanUp(UINT64 CompletedFenceValue);

    inline ID3D12DescriptorHeap* GetHeap() const { return Heap.Get(); }
	inline UINT GetDescriptorSize() const { return DescriptorSize; }

private:
    ComPtr<ID3D12DescriptorHeap> Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapCpuStart = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE HeapGpuStart = { 0 };

    UINT DescriptorSize = 0;
    UINT TotalDescriptorCount = 0;
	UINT OriginalDescriptorCount = 0;
    UINT NextFreeIndex = 0;

    std::vector<UINT> FreeList;
    std::queue<DeferredFreeItem> DeferredQueue; // Queue maintains strict temporal FIFO order
    std::mutex Mutex;

	std::vector<SReservedBlock> ReservedBlocks;
};

