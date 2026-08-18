#include "DescriptorAllocator.h"
#include "Logger.h"
#include "Renderer.h"


void CDescriptorAllocator::Init(ID3D12Device* Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptors)
{
    NumDescriptors_ = NumDescriptors;
    DescriptorSize = Device->GetDescriptorHandleIncrementSize(Type);

    D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
    Desc.Type = Type;
    Desc.NumDescriptors = NumDescriptors;
    Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Desc.NodeMask = 0;

    HRESULT Hr = Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap));
    assert(SUCCEEDED(Hr));

    HeapCpuStart = Heap->GetCPUDescriptorHandleForHeapStart();
    HeapGpuStart = Heap->GetGPUDescriptorHandleForHeapStart();
    NextFreeIndex = 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorAllocator::BeginBlockAllocation()
{
    if(bIsBlockAllocating)
    {
        LOG_ERROR("Block allocation already in progress!");
		return { 0 };
	}

	bIsBlockAllocating = true;

	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = HeapGpuStart;
    GpuHandle.ptr += NextFreeIndex * DescriptorSize;
	return GpuHandle;
}

SDescriptorHandle CDescriptorAllocator::Allocate()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    uint32_t Index = 0;

    if (!FreeList.empty() && !bIsBlockAllocating) 
    {
        Index = FreeList.back();
        FreeList.pop_back();
    }
    else 
    {
        assert(NextFreeIndex < NumDescriptors_ && "Allocator out of descriptors!");
        Index = NextFreeIndex++;
    }

    SDescriptorHandle Handle;

    Handle.CpuHandle = HeapCpuStart;
    Handle.CpuHandle.ptr += Index * DescriptorSize;

    Handle.GpuHandle = HeapGpuStart;
    Handle.GpuHandle.ptr += Index * DescriptorSize;

    return Handle;
}

void CDescriptorAllocator::DeferredFree(D3D12_CPU_DESCRIPTOR_HANDLE InCpuHandle)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    
    // Validate the handle is in the heap's range
    if (InCpuHandle.ptr < HeapCpuStart.ptr || InCpuHandle.ptr >= HeapCpuStart.ptr + NumDescriptors_ * DescriptorSize)
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

    uint32_t Index = static_cast<uint32_t>((InCpuHandle.ptr - HeapCpuStart.ptr) / DescriptorSize);
    assert(Index < NumDescriptors_);

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

        FreeList.push_back(Item.HeapIndex);
        DeferredQueue.pop();
    }
}

