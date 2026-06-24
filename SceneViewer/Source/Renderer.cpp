#include "Renderer.h"
#include "Scene.h"
#include "DDSTextureLoader12.h"

CUniformBuffer::CUniformBuffer()
{
}

void CUniformBuffer::Init(UINT InEleSize, UINT InEleCount)
{
    ElementSize = InEleSize;
    ElementCount = InEleCount;

    CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(InEleSize * InEleCount);

    CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Buffer));

    Buffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedPtr));
}

CUniformBuffer::~CUniformBuffer()
{
    Buffer->Unmap(0, nullptr);
    MappedPtr = nullptr;
}

void CUniformBuffer::CreateShaderResourceView()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Buffer.FirstElement = 0;
    SrvDesc.Buffer.NumElements = ElementCount;
    SrvDesc.Buffer.StructureByteStride = ElementSize;
    SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor = CRenderer::GetInstance().AllocSrvDescriptor(SrvDescriptorIndex);
    CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(Buffer.Get(), &SrvDesc, Descriptor);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CUniformBuffer::GetSrvGPUDescriptor()
{
    if (SrvDescriptorIndex < 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE DefaultHandle = {};
        return DefaultHandle;
    }

    return CRenderer::GetInstance().GetSrvGPUDescriptor(SrvDescriptorIndex);
}

D3D12_GPU_VIRTUAL_ADDRESS CUniformBuffer::GetGPUAddress()
{
    return Buffer->GetGPUVirtualAddress();
}

void CUniformBuffer::SetData(void* InData)
{
    if (InData == nullptr)
    {
        return;
    }

    memcpy(MappedPtr, InData, ElementSize * ElementCount);
}

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

    D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
    SrvHeapDesc.NumDescriptors = 1024;
    SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    SrvHeapDesc.NodeMask = 0;
    D3dDevice->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&SrvDescriptorHeap));
    SrvDescriptorSize = D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc = {};
    RtvHeapDesc.NumDescriptors = 32;
    RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RtvHeapDesc.NodeMask = 0;
    D3dDevice->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&RtvDescriptorHeap));
    RtvDescriptorSize = D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc = {};
    DsvHeapDesc.NumDescriptors = 3;
    DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DsvHeapDesc.NodeMask = 0;
    D3dDevice->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&DsvDescriptorHeap));
    DsvDescriptorSize = D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    CreateDepthTexture(L"Depth", ViewportWidth, ViewportHeight);

    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP));

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

        PerFrameContext[i].ViewBuffer.Init((UINT)(sizeof(SViewBuffer)), 1);
    }

    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = static_cast<float>(ViewportWidth);
    Viewport.Height = static_cast<float>(ViewportHeight);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    ScissorRect = CD3DX12_RECT(0, 0, ViewportWidth, ViewportHeight);

    D3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, PerFrameContext[CurrentFrameIndex].CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
    CommandList->Close();

    Scene = std::make_unique<CScene>();

    LoadScene();

    return true;
}

void	CRenderer::LoadScene()
{
    //// load scene
    GetCurrentFrameContext().CommandAllocator->Reset();
    CommandList->Reset(GetCurrentFrameContext().CommandAllocator.Get(), nullptr);

    Scene->Load();

    CommandList->Close();

    ID3D12CommandList* CmdLists[] = { CommandList.Get() };
    D3DCommandQueue->ExecuteCommandLists(1, CmdLists);

    FlushCommandQueue();
    ///
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

void	CRenderer::UpdateViewBuffer()
{
    if (Scene == nullptr)
    {
        return;
    }

    CCamera* Cam = Scene->GetMainCamera();
    Cam->OnUpdate();

    Cam->GetCameraPosition(&(ViewBuffer.CameraOrigin));
    Cam->GetViewMatrix(&(ViewBuffer.ViewMatrix));
    Cam->GetProjectionMatrix(&(ViewBuffer.ProjectionMatrix));

    GetCurrentFrameContext().ViewBuffer.SetData(&ViewBuffer);

    CommandList->SetGraphicsRootConstantBufferView(0, GetCurrentFrameContext().ViewBuffer.GetGPUAddress());
}

void	CRenderer::Render()
{
    BeginFrame();

    CommandList->RSSetViewports(1, &Viewport);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CMaterial* SceneMaterial = Scene->GetSceneMaterial();
    if (SceneMaterial)
    {
        SceneMaterial->OnRender(CommandList.Get());
    }

    ID3D12DescriptorHeap* Heaps[] = { SrvDescriptorHeap.Get() };
    CommandList->SetDescriptorHeaps(1, Heaps);

    CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), CurrentFrameIndex, RtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE DsvHandle(DsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    CommandList->OMSetRenderTargets(1, &RtvHandle, true, &DsvHandle);

    float ClearColor[] = { 0.0f, 0.1f, 0.5f, 1.0f };
    CommandList->ClearRenderTargetView(RtvHandle, ClearColor, 0, nullptr);
    CommandList->ClearDepthStencilView(DsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    UpdateViewBuffer();

    Scene->OnRender(CommandList.Get());

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

ComPtr<ID3D12Resource> CRenderer::CreateDefaultBuffer(const void* InData, UINT InTotalByteSize, ComPtr<ID3D12Resource>& OutUploadBuffer)
{
    ComPtr<ID3D12Resource> Buffer;

    CD3DX12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC ResDesc = CD3DX12_RESOURCE_DESC::Buffer(InTotalByteSize);
    D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&Buffer));

    CD3DX12_HEAP_PROPERTIES UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3dDevice->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(OutUploadBuffer.GetAddressOf()));

    D3D12_SUBRESOURCE_DATA SubResourceData = {};
    SubResourceData.pData = InData;
    SubResourceData.RowPitch = InTotalByteSize;
    SubResourceData.SlicePitch = InTotalByteSize;

    // dx12 force the buffer created as COMMON
    ResourceBarrier(Buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    UpdateSubresources<1>(CommandList.Get(), Buffer.Get(), OutUploadBuffer.Get(), 0, 0, 1, &SubResourceData);
    ResourceBarrier(Buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

    return Buffer;
}

std::filesystem::path CRenderer::GetExeDirectory()
{
    WCHAR PathCharArray[MAX_PATH];
    GetModuleFileNameW(NULL, PathCharArray, MAX_PATH);
    std::filesystem::path ExePath(PathCharArray);
    return ExePath.parent_path();
}

CTexture2D* CRenderer::GetTexture(LPCWSTR InFileName)
{
    if (AllTextures.find(InFileName) != AllTextures.end())
    {
        return AllTextures[InFileName].get();
    }

    return nullptr;
}

CTexture2D* CRenderer::CreateDepthTexture(LPCWSTR InName, UINT InW, UINT InH)
{
    if (AllTextures.find(InName) != AllTextures.end())
    {
        return AllTextures[InName].get();
    }

    std::unique_ptr<CTexture2D> NewTexture = std::make_unique<CTexture2D>();

    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Use typeless so it can be cast to DSV and SRV
    TextureDesc.Width = InW;
    TextureDesc.Height = InH;
    TextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    ClearValue.DepthStencil.Depth = 1.0f;
    ClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(NewTexture->Texture.GetAddressOf()));

    CTexture2D* ResTex = NewTexture.get();
    NewTexture->Width = InW;
    NewTexture->Height = InH;

    D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
    DsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // Cast from R32_TYPELESS
    DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    DsvDesc.Texture2D.MipSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = DsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3dDevice->CreateDepthStencilView(ResTex->Texture.Get(), &DsvDesc, DsvHandle);

    NewTexture->CreateShaderResourceView();

    AllTextures[InName] = std::move(NewTexture);
    return ResTex;
}

CTexture2D* CRenderer::LoadTexture(LPCWSTR InFileName)
{
    if (AllTextures.find(InFileName) != AllTextures.end())
    {
        return AllTextures[InFileName].get();
    }

    std::unique_ptr<CTexture2D> NewTexture = std::make_unique<CTexture2D>();

    std::vector<D3D12_SUBRESOURCE_DATA> Subresources;

    std::filesystem::path ExeDirectory = CRenderer::GetExeDirectory();
    std::filesystem::path AssetDir = ExeDirectory.parent_path().parent_path();
    AssetDir /= L"SceneViewer/Asset";
    AssetDir /= InFileName;

    if (FAILED(LoadDDSTextureFromFile(D3dDevice.Get(), AssetDir.c_str(), NewTexture->Texture.GetAddressOf(), NewTexture->DDSData, Subresources)))
    {
        return nullptr;
    }

    UINT64 ReqSize = GetRequiredIntermediateSize(NewTexture->Texture.Get(), 0, static_cast<UINT>(Subresources.size()));

    CD3DX12_HEAP_PROPERTIES UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC ResDesc = CD3DX12_RESOURCE_DESC::Buffer(ReqSize);

    D3dDevice->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(NewTexture->UploadTexture.GetAddressOf()));

    UpdateSubresources(CommandList.Get(), NewTexture->Texture.Get(), NewTexture->UploadTexture.Get(), 0, 0, static_cast<UINT>(Subresources.size()), Subresources.data());
    ResourceBarrier(NewTexture->Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CTexture2D* ResTex = NewTexture.get();

    NewTexture->Width = NewTexture->Texture->GetDesc().Width;
    NewTexture->Height = NewTexture->Texture->GetDesc().Height;

    NewTexture->CreateShaderResourceView();

    AllTextures[InFileName] = std::move(NewTexture);
    return ResTex;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CRenderer::AllocSrvDescriptor(int& OutDescriptorIdx)
{
    OutDescriptorIdx = CurrentSrvDescriptorIndex;
    CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor(SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    Descriptor.Offset(CurrentSrvDescriptorIndex, SrvDescriptorSize);
    CurrentSrvDescriptorIndex++;
    return Descriptor;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CRenderer::GetSrvGPUDescriptor(UINT Idx)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE Descriptor(SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    Descriptor.Offset(Idx, SrvDescriptorSize);
    return Descriptor;
}