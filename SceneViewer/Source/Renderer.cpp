#include "Renderer.h"

CRenderer& CRenderer::GetInstance()
{
    static CRenderer TheInstance;
    return TheInstance;    
}

bool	CRenderer::Init(HWND hWnd)
{
    UINT DXgiFactoryFlags = 0;

#if defined _DEBUG
    ComPtr<ID3D12Debug> D3dDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&D3dDebug))))
    {
        D3dDebug->EnableDebugLayer();
        DXgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIAdapter1> DXgiAdapter;
    ComPtr<IDXGIFactory4> DXgiFactory;
    if (FAILED(CreateDXGIFactory2(DXgiFactoryFlags, IID_PPV_ARGS(&DXgiFactory))))
    {
        return false;
    }

    for (UINT i = 0; SUCCEEDED(DXgiFactory->EnumAdapters1(i, &DXgiAdapter)); ++i)
    {
        DXGI_ADAPTER_DESC1 AdapterDesc;
        DXgiAdapter->GetDesc1(&AdapterDesc);

        if (AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(DXgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&D3dDevice))))
        {
            break;
        }
    }

    if (D3dDevice.Get() == nullptr)
    {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(D3dDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&D3DCommandQueue))))
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.BufferCount = TotalFrameCount;
    SwapChainDesc.Width = ViewportWidth;
    SwapChainDesc.Height = ViewportHeight;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.SampleDesc.Count = 1;

    ComPtr< IDXGISwapChain1> SwapChain1;
    if (FAILED(DXgiFactory->CreateSwapChainForHwnd(D3DCommandQueue.Get(), hWnd, &SwapChainDesc, nullptr, nullptr, &SwapChain1)))
    {
        return false;
    }

    if (FAILED(SwapChain1.As(&SwapChain)))
    {
        return false;
    }

    DXgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    CurrentFrameIndex = SwapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc;
    RtvHeapDesc.NumDescriptors = 32;
    RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RtvHeapDesc.NodeMask = 0;
    D3dDevice->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&RtvDescriptorHeap));

    RtvDescriptorSize = D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence));
    FrameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    PerFrameContext[CurrentFrameIndex].FenceValue = 1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < TotalFrameCount; ++i)
    {
        D3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&(PerFrameContext[i].CommandAllocator)));

        SwapChain->GetBuffer(i, IID_PPV_ARGS(&(PerFrameContext[i].FrameBuffer)));
        D3dDevice->CreateRenderTargetView(PerFrameContext[i].FrameBuffer.Get(), nullptr, RtvHandle);
        RtvHandle.Offset(1, RtvDescriptorSize);
    }

    D3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, PerFrameContext[CurrentFrameIndex].CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
    CommandList->Close();

    return true;
}

void	CRenderer::BeginFrame()
{
    GetCurrentFrameContext().CommandAllocator->Reset();
    CommandList->Reset(GetCurrentFrameContext().CommandAllocator.Get(), nullptr);

    ResourceBarrier(GetCurrentFrameContext().FrameBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void	CRenderer::EndFrame()
{
    ResourceBarrier(GetCurrentFrameContext().FrameBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    CommandList->Close();

    ID3D12CommandList* CmdLists[] = { CommandList.Get() };
    D3DCommandQueue->ExecuteCommandLists(1, CmdLists);

    SwapChain->Present(0, 0);

    UINT64 FenceValue = GetCurrentFrameContext().FenceValue;
    D3DCommandQueue->Signal(FrameFence.Get(), FenceValue);

    CurrentFrameIndex = SwapChain->GetCurrentBackBufferIndex();

    if (FrameFence->GetCompletedValue() < GetCurrentFrameContext().FenceValue)
    {
        FrameFence->SetEventOnCompletion(GetCurrentFrameContext().FenceValue, FrameFenceEvent);
        WaitForSingleObjectEx(FrameFenceEvent, INFINITE, FALSE);
    }

    GetCurrentFrameContext().FenceValue = (FenceValue + 1);
}

void	CRenderer::Render()
{
    BeginFrame();

    CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), CurrentFrameIndex, RtvDescriptorSize);
    CommandList->OMSetRenderTargets(1, &RtvHandle, FALSE, nullptr);

    float ClearColor[] = { 0.0f, 0.1f, 0.5f, 1.0f };
    CommandList->ClearRenderTargetView(RtvHandle, ClearColor, 0, nullptr);

    EndFrame();
}

void CRenderer::ResourceBarrier(ID3D12Resource* InResource, D3D12_RESOURCE_STATES InBefore, D3D12_RESOURCE_STATES InAfter)
{
    CD3DX12_RESOURCE_BARRIER ResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(InResource, InBefore, InAfter);
    CommandList->ResourceBarrier(1, &ResBarrier);
}

void	CRenderer::FlushCommandQueue()
{
    SPerFrameContext& CurContext = GetCurrentFrameContext();
    UINT64 FenceValue = CurContext.FenceValue;
    D3DCommandQueue->Signal(FrameFence.Get(), FenceValue);

    FrameFence->SetEventOnCompletion(GetCurrentFrameContext().FenceValue, FrameFenceEvent);
    WaitForSingleObjectEx(FrameFenceEvent, INFINITE, FALSE);

    CurContext.FenceValue = (FenceValue + 1);
}

void	CRenderer::Shutdown()
{
    FlushCommandQueue();
    CloseHandle(FrameFenceEvent);
}