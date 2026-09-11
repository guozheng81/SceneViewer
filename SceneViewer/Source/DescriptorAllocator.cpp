#include "DescriptorAllocator.h"
#include "Logger.h"
#include "Renderer.h"


void CDescriptorAllocator::Init(ID3D12Device* Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT NumDescriptors, bool ShaderVisible)
{
    TotalDescriptorCount = NumDescriptors;
	OriginalDescriptorCount = NumDescriptors;
    DescriptorSize = Device->GetDescriptorHandleIncrementSize(Type);

    D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
    Desc.Type = Type;
    Desc.NumDescriptors = NumDescriptors;
    Desc.Flags = ShaderVisible? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Desc.NodeMask = 0;

    HRESULT Hr = Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap));
    assert(SUCCEEDED(Hr));

    HeapCpuStart = Heap->GetCPUDescriptorHandleForHeapStart();

	bIsShaderVisible = ShaderVisible;
    if (ShaderVisible)
    {
        HeapGpuStart = Heap->GetGPUDescriptorHandleForHeapStart();
    }
    NextFreeIndex = 0;
	ReservedBlocks.clear();
}

UINT CDescriptorAllocator::ReserveBlock(UINT InStartOffset, UINT NumDescriptors)
{
    std::lock_guard<std::mutex> Lock(Mutex);

	// Validate the requested block is within the heap's range and it does not overlap with existing reserved blocks
    if (InStartOffset + NumDescriptors > OriginalDescriptorCount)
    {
        LOG_ERROR("Reserved block exceeds heap range!");
        return static_cast<UINT>(-1);
    }

    for (const SReservedBlock& ExistingBlock : ReservedBlocks)
    {
        bool bOverlaps = InStartOffset < ExistingBlock.StartOffset + ExistingBlock.NumDescriptors &&
            ExistingBlock.StartOffset < InStartOffset + NumDescriptors;

        if (bOverlaps)
        {
            LOG_ERROR("Reserved block overlaps with an existing reserved block!");
            return static_cast<UINT>(-1);
        }
    }

    SReservedBlock Block;
    Block.StartOffset = InStartOffset;
    Block.NumDescriptors = NumDescriptors;
    Block.NextFreeIndex = InStartOffset;

    ReservedBlocks.push_back(Block);

	TotalDescriptorCount = std::min(TotalDescriptorCount, InStartOffset);

    return static_cast<UINT>(ReservedBlocks.size() - 1);
}

void CDescriptorAllocator::ResetReservedBlock(UINT BlockIndex)
{
    std::lock_guard<std::mutex> Lock(Mutex);

    assert(BlockIndex < ReservedBlocks.size() && "Invalid reserved block index!");

    ReservedBlocks[BlockIndex].NextFreeIndex = ReservedBlocks[BlockIndex].StartOffset;
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorAllocator::GetReservedBlockGpuHandle(UINT BlockIndex) const
{
    if (BlockIndex >= ReservedBlocks.size())
    {
        LOG_ERROR("Invalid reserved block index!");
        return { 0 };
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = HeapGpuStart;
    GpuHandle.ptr += ReservedBlocks[BlockIndex].StartOffset * DescriptorSize;
    return GpuHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorAllocator::BeginBlockAllocation(UINT BlockIndex)
{
    if (ActiveBlockIndex >= 0)
    {
        LOG_ERROR("Block allocation already in progress!");
		return { 0 };
	}

    if (BlockIndex >= ReservedBlocks.size())
    {
        LOG_ERROR("Invalid reserved block index!");
        return { 0 };
    }

	ActiveBlockIndex = static_cast<int>(BlockIndex);

	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = HeapGpuStart;
    GpuHandle.ptr += ReservedBlocks[BlockIndex].NextFreeIndex * DescriptorSize;
	return GpuHandle;
}

SDescriptorHandle CDescriptorAllocator::Allocate()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    UINT Index = 0;

    if (ActiveBlockIndex >= 0)
    {
        assert(static_cast<size_t>(ActiveBlockIndex) < ReservedBlocks.size());

        SReservedBlock& Block = ReservedBlocks[ActiveBlockIndex];
        assert(Block.NextFreeIndex < Block.StartOffset + Block.NumDescriptors && "Reserved block out of descriptors!");

        Index = Block.NextFreeIndex++;
    }
    else if (!FreeList.empty())
    {
        Index = FreeList.back();
        FreeList.pop_back();
    }
    else 
    {
        assert(NextFreeIndex < TotalDescriptorCount && "Allocator out of descriptors!");
        Index = NextFreeIndex++;
    }

    SDescriptorHandle Handle;

    Handle.CpuHandle = HeapCpuStart;
    Handle.CpuHandle.ptr += Index * DescriptorSize;

    if (bIsShaderVisible)
    {
        Handle.GpuHandle = HeapGpuStart;
        Handle.GpuHandle.ptr += Index * DescriptorSize;
    }

    return Handle;
}

void CDescriptorAllocator::DeferredFree(D3D12_CPU_DESCRIPTOR_HANDLE InCpuHandle)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    
    // Validate the handle is in the heap's range
    if (InCpuHandle.ptr < HeapCpuStart.ptr || InCpuHandle.ptr >= HeapCpuStart.ptr + TotalDescriptorCount * DescriptorSize)
    {
        LOG_ERROR("Invalid CPU descriptor handle for this heap!");
        return;
    }

    // Validate alignment: handle must align to descriptor boundaries
    if ((InCpuHandle.ptr - HeapCpuStart.ptr) % DescriptorSize != 0)
    {
        LOG_ERROR("CPU descriptor handle is not aligned to descriptor boundary!");
        return;
    }

    UINT Index = static_cast<UINT>((InCpuHandle.ptr - HeapCpuStart.ptr) / DescriptorSize);
    assert(Index < TotalDescriptorCount);

    UINT64 FenceValueToWait = CRenderer::GetInstance().GetCurrentFrameContext().FenceValue + 1;
    DeferredQueue.push({ Index, FenceValueToWait });
}

void CDescriptorAllocator::CleanUp(UINT64 CompletedFenceValue)
{
    std::lock_guard<std::mutex> Lock(Mutex);

    while (!DeferredQueue.empty()) {
        const auto& Item = DeferredQueue.front();

        // Queue is ordered by time; if the oldest item hasn't finished, later ones haven't either
        if (CompletedFenceValue < Item.FenceValue) {
            break;
        }

        FreeList.push_back(Item.Offset);
        DeferredQueue.pop();
    }
}

