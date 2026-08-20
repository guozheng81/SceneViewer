#include "Renderer.h"
#include "Scene.h"
#include "DDSTextureLoader12.h"
#include "ScreenPass.h"
#include "Logger.h"
#include "Texture.h"

CBuffer::CBuffer()
{
}

void CBuffer::Init(UINT InEleSize, UINT InEleCount, bool InForUpload, D3D12_RESOURCE_STATES InInitState, bool bNeedUAV, bool bConstantBuffer)
{
    if (InEleSize == 0 || InEleCount == 0)
    {
        LOG_ERROR("CBuffer::Init: Invalid element size (%u) or count (%u).", InEleSize, InEleCount);
        return;
    }

    ElementSize = InEleSize;
    ElementCount = InEleCount;
    bUseForUpload = InForUpload;
    bIsConstantBuffer = bConstantBuffer;

    if (bIsConstantBuffer)
    {
        UINT AlignedElementSize = (ElementSize + 255) & ~255;
        if (AlignedElementSize != ElementSize)
        {
            LOG_WARN("CBuffer::Init: Constant buffer size %u aligned to %u bytes.", ElementSize, AlignedElementSize);
            ElementSize = AlignedElementSize;
        }
    }

    CD3DX12_HEAP_PROPERTIES HeapProps(bUseForUpload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(InEleSize * InEleCount);

    if (bNeedUAV)
    {
        BufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    HRESULT HrCreateResource = CRenderer::GetInstance().D3dDevice->CreateCommittedResource(
        &HeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, InInitState, nullptr, IID_PPV_ARGS(&Buffer));

    if (FAILED(HrCreateResource))
    {
        LOG_ERROR("CBuffer::Init: CreateCommittedResource failed (0x%08X).", HrCreateResource);
        return;
    }

    if (bUseForUpload)
    {
        HRESULT HrMap = Buffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedPtr));
        if (FAILED(HrMap))
        {
            LOG_ERROR("CBuffer::Init: Failed to map buffer (0x%08X).", HrMap);
            Buffer.Reset();
            return;
        }
    }

    LOG_INFO("CBuffer::Init: Buffer created (Size: %u bytes, Elements: %u, Upload: %s).",
        InEleSize * InEleCount, InEleCount, bUseForUpload ? "true" : "false");
}

CBuffer::~CBuffer()
{
    ResetMappedData();
}

void CBuffer::ResetMappedData()
{
    if (bUseForUpload && MappedPtr != nullptr)
    {
        Buffer->Unmap(0, nullptr);
        MappedPtr = nullptr;
    }
}

void CBuffer::Reset()
{
    ResetMappedData();

    if (Buffer)
    {
        Buffer.Reset();
    }
}

void CBuffer::SetElementData(UINT Idx, void* InData, UINT InSize)
{
    if (InData == nullptr)
    {
        LOG_ERROR("CBuffer::SetElementData: Input data is null.");
        return;
    }

    if (Idx >= ElementCount)
    {
        LOG_ERROR("CBuffer::SetElementData: Index %u exceeds element count %u.", Idx, ElementCount);
        return;
    }

    if (MappedPtr == nullptr)
    {
        LOG_ERROR("CBuffer::SetElementData: Buffer is not mapped (not an upload buffer).");
        return;
    }

    UINT ActualSize = (InSize == 0) ? ElementSize : InSize;
    if (ActualSize > ElementSize)
    {
        LOG_WARN("CBuffer::SetElementData: Input size %u exceeds element size %u, truncating.", ActualSize, ElementSize);
        ActualSize = ElementSize;
    }

    memcpy(MappedPtr + ElementSize * Idx, InData, ActualSize);
}

void CBuffer::CreateShaderResourceView()
{
    if (!Buffer.Get())
    {
        LOG_ERROR("CBuffer::CreateShaderResourceView: Buffer resource is null.");
        return;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Buffer.FirstElement = 0;
    SrvDesc.Buffer.NumElements = ElementCount;
    SrvDesc.Buffer.StructureByteStride = ElementSize;
    SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    SDescriptorHandle DescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
    SrvCPUDescriptor = DescriptorHandle.CpuHandle;
    SrvGPUDescriptor = DescriptorHandle.GpuHandle;

    CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(Buffer.Get(), &SrvDesc, SrvCPUDescriptor);
}

D3D12_GPU_VIRTUAL_ADDRESS CBuffer::GetGPUAddress(UINT InIdx)
{
    if (InIdx >= ElementCount)
    {
        LOG_ERROR("CBuffer::GetGPUAddress: Index %u exceeds element count %u.", InIdx, ElementCount);
        return 0;
    }

    if (!Buffer.Get())
    {
        LOG_ERROR("CBuffer::GetGPUAddress: Buffer resource is null.");
        return 0;
    }

    return Buffer->GetGPUVirtualAddress() + InIdx * ElementSize;
}

void CBuffer::SetData(void* InData)
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
	CLogger::GetInstance().Init((GetExeDirectory()/"Log.txt").string());
	LOG_INFO("Initializing renderer (Viewport %u %u)", ViewportWidth, ViewportHeight);

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
        LOG_ERROR("CreateDXGIFactory failed");
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
        LOG_ERROR("No suitable D3D12 device found");
        return false;
    }

    //////// command queue /////////////////

    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(D3dDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&D3DCommandQueue))))
    {
        LOG_ERROR("CreateCommandQueue failed");
        return false;
    }

    //////// swap chain /////////////////

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
		LOG_ERROR("CreateSwapChainForHwnd failed");
        return false;
    }

    if (FAILED(SwapChain1.As(&SwapChain)))
    {
        return false;
    }

    DXgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    CurrentFrameIndex = SwapChain->GetCurrentBackBufferIndex();

    //////////// descriptor heaps /////////////////

	SrvUavDescriptorAllocator.Init(D3dDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);

	RtvDescriptorAllocator.Init(D3dDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32, false);
	DsvDescriptorAllocator.Init(D3dDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 4, false);

    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
    TextureSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP));

    /////////////////// per frame resources ///////////////////

    D3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence));
    FrameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    PerFrameContext[CurrentFrameIndex].FenceValue = 1;

    D3D12_RENDER_TARGET_VIEW_DESC FrameBufferRtvDesc = {};
    FrameBufferRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // Apply gamma correction
    FrameBufferRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    for (UINT i = 0; i < TotalFrameCount; ++i)
    {
        D3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&(PerFrameContext[i].CommandAllocator)));

        SwapChain->GetBuffer(i, IID_PPV_ARGS(&(PerFrameContext[i].FrameBuffer)));

		D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = RtvDescriptorAllocator.Allocate().CpuHandle;
        D3dDevice->CreateRenderTargetView(PerFrameContext[i].FrameBuffer.Get(), &FrameBufferRtvDesc, RtvHandle);
        PerFrameContext[i].FrameBufferRtvDescriptor = RtvHandle;

        PerFrameContext[i].ViewBuffer.Init((UINT)(sizeof(SViewBuffer)), 1, true, D3D12_RESOURCE_STATE_GENERIC_READ, false, true);
    }

    ///////////////////////////////////////////////

    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = static_cast<float>(ViewportWidth);
    Viewport.Height = static_cast<float>(ViewportHeight);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    ScissorRect = CD3DX12_RECT(0, 0, ViewportWidth, ViewportHeight);

    D3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, PerFrameContext[CurrentFrameIndex].CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
    CommandList->Close();

    /////////////////////////////////////////////////

    Scene = std::make_unique<CScene>();

    LoadScene();

    Scene->OnLoaded();
    ScreenQuad->ResetUploadResource();
    for (auto& CurTexture : AllTextures)
    {
        CurTexture.second->ResetUploadResource();
    }

    Scene->SetDirectionalLight(XMFLOAT3(-0.3f, -1.0f, -0.15f), 10.0f);

    //ScreenPasses.push_back(std::make_unique<CSimpleRTPass>());
    ScreenPasses.push_back(std::make_unique<CShadowRTPass>());
    ScreenPasses.push_back(std::make_unique<CIndirectLightRTPass>());
    ScreenPasses.push_back(std::make_unique<CLightPass>());

    for (auto& Pass : ScreenPasses)
    {
        Pass->Init();
    }

    LOG_INFO("Renderer initialization complete");
    return true;
}

void	CRenderer::LoadScene()
{
    //// load scene
    GetCurrentFrameContext().CommandAllocator->Reset();
    CommandList->Reset(GetCurrentFrameContext().CommandAllocator.Get(), nullptr);

    Scene->Load("sponza.obj", CommandList.Get());

    ScreenQuad = std::make_unique<CMesh>();

    std::vector<SSceneVertex> Verts = {
        { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
        { { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
        { { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} }
    };

    std::vector<UINT32>	Indices = { 0, 1, 2, 0, 3, 1 };
    ScreenQuad->Init(Verts, Indices);

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
}

void	CRenderer::EndFrame()
{
    if (bIsFirstFrame)
    {
        bIsFirstFrame = false;
    }

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

	SrvUavDescriptorAllocator.CleanUp(FrameFence->GetCompletedValue());

    GetCurrentFrameContext().FenceValue = (FenceValue + 1);
}

void	CRenderer::UpdateViewBuffer()
{
    if (Scene == nullptr)
    {
        return;
    }

    AccumulatedFrameNumber++;

    ViewBuffer.PrevViewProjectionMatrix = ViewBuffer.ViewProjectionMatrix;

    CCamera* Cam = Scene->GetMainCamera();
    Cam->OnUpdate();

    Cam->GetCameraPosition(&(ViewBuffer.CameraOrigin));
    Cam->UpdateViewBuffer(&ViewBuffer);

    if (bIsFirstFrame)
    {
        ViewBuffer.PrevViewProjectionMatrix = ViewBuffer.ViewProjectionMatrix;
    }

    XMFLOAT3 LightDir;
    XMStoreFloat3(&LightDir, Scene->DirectionalLightDir);
    ViewBuffer.DirectionalLight = XMFLOAT4(-LightDir.x, -LightDir.y, -LightDir.z, Scene->DirectionalLightIntensity);
    ViewBuffer.FrameNumber = AccumulatedFrameNumber;
    ViewBuffer.ViewportSize = XMFLOAT4(ViewportWidth, ViewportHeight, 1.0f / (float)ViewportWidth, 1.0f / (float)ViewportHeight);

    float Near = Cam->GetNearPlane();
    float Far = Cam->GetFarPlane();
    ViewBuffer.Proj_m22 = Far / (Far - Near);
    ViewBuffer.Proj_m32 = (-Far) * Near / (Far - Near);

    GetCurrentFrameContext().ViewBuffer.SetData(&ViewBuffer);
}

void	CRenderer::Render()
{
    BeginFrame();

    CommandList->RSSetViewports(1, &Viewport);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    ID3D12DescriptorHeap* Heaps[] = { SrvUavDescriptorAllocator.GetHeap()};
    CommandList->SetDescriptorHeaps(1, Heaps);

    UpdateViewBuffer();

    Scene->OnRender(CommandList.Get());

    for (auto& Pass : ScreenPasses)
    {
        Pass->OnRender(CommandList.Get());
    }

    EndFrame();
}

void CRenderer::ResourceBarrier(ID3D12Resource* InResource, D3D12_RESOURCE_STATES InBefore, D3D12_RESOURCE_STATES InAfter)
{
    CD3DX12_RESOURCE_BARRIER ResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(InResource, InBefore, InAfter);
    CommandList->ResourceBarrier(1, &ResBarrier);
}

void	CRenderer::FlushCommandQueue(bool bShouldIncreaseFence)
{
    SPerFrameContext& CurContext = GetCurrentFrameContext();
    UINT64 FenceValue = CurContext.FenceValue;
    D3DCommandQueue->Signal(FrameFence.Get(), FenceValue);

    FrameFence->SetEventOnCompletion(GetCurrentFrameContext().FenceValue, FrameFenceEvent);
    WaitForSingleObjectEx(FrameFenceEvent, INFINITE, FALSE);

    if (bShouldIncreaseFence)
    {
        CurContext.FenceValue = (FenceValue + 1);
    }
}

void	CRenderer::Shutdown()
{
    FlushCommandQueue();
    CloseHandle(FrameFenceEvent);

    LOG_INFO("Renderer Shutdown");
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

std::filesystem::path CRenderer::GetAssetDirectory()
{
    std::filesystem::path ExeDirectory = CRenderer::GetExeDirectory();
    std::filesystem::path AssetDir = ExeDirectory.parent_path().parent_path();
    AssetDir /= "SceneViewer/Asset";

    return AssetDir;
}

CTexture* CRenderer::GetTexture(const std::string& InFileName)
{
    if (AllTextures.find(InFileName) != AllTextures.end())
    {
        return AllTextures[InFileName].get();
    }

    return nullptr;
}

CTextureDepthStencil* CRenderer::CreateDepthTexture(const std::string& InName, UINT InW, UINT InH)
{
    if (AllTextures.find(InName) != AllTextures.end())
    {
        return dynamic_cast<CTextureDepthStencil*>(AllTextures[InName].get());
    }

    std::unique_ptr<CTextureDepthStencil> NewTexture = std::make_unique<CTextureDepthStencil>(DXGI_FORMAT_R32_TYPELESS, InW, InH);
    NewTexture->CreateResource();
	NewTexture->CreateDepthStencilView();

    NewTexture->CreateShaderResourceView();

    CTextureDepthStencil* ResTex = NewTexture.get();
    AllTextures[InName] = std::move(NewTexture);
    return ResTex;
}

CTexture2D* CRenderer::LoadTexture(const std::string& InFileName, bool InIsDiffuse)
{
    if (AllTextures.find(InFileName) != AllTextures.end())
    {
        return dynamic_cast<CTexture2D*>(AllTextures[InFileName].get());
    }

    std::unique_ptr<CTexture2D> NewTexture = std::make_unique<CTexture2D>(InIsDiffuse);

    std::filesystem::path AssetDir = CRenderer::GetAssetDirectory();
    std::filesystem::path TexFileName = AssetDir / InFileName;
    if (!std::filesystem::exists(TexFileName))
    {
        TexFileName = AssetDir / "default_n.dds";
    }

	NewTexture->LoadResource(TexFileName.c_str(), CommandList.Get());
    NewTexture->CreateShaderResourceView();

    CTexture2D* ResTex = NewTexture.get();
    AllTextures[InFileName] = std::move(NewTexture);
    return ResTex;
}

CTextureRenderTarget* CRenderer::CreateRenderTarget(const std::string& InName, DXGI_FORMAT InFormat, XMFLOAT4 InColor, UINT InW, UINT InH, bool InNeedRtv, bool InNeedUav)
{
    if (AllTextures.find(InName) != AllTextures.end())
    {
        return dynamic_cast<CTextureRenderTarget*>(AllTextures[InName].get());
    }

    std::unique_ptr<CTextureRenderTarget> NewTexture = std::make_unique<CTextureRenderTarget>(InFormat, InColor, (InW != 0 ? InW : ViewportWidth), (InH != 0 ? InH : ViewportHeight), InNeedRtv, InNeedUav);
    NewTexture->CreateResource();
    if (InNeedRtv)
    {
        NewTexture->CreateRenderTargetView();
    }
    if (InNeedUav)
    {
        NewTexture->CreateUnorderedAccessView();
    }
    NewTexture->CreateShaderResourceView();

    CTextureRenderTarget* ResTex = NewTexture.get();
    AllTextures[InName] = std::move(NewTexture);
    return ResTex;
}

int	CRenderer::GetSrvDescriptorOffset(CD3DX12_GPU_DESCRIPTOR_HANDLE InStart, CD3DX12_GPU_DESCRIPTOR_HANDLE InEnd)
{
    return (int)(InEnd.ptr - InStart.ptr) / (int)SrvUavDescriptorAllocator.GetDescriptorSize();
}

void	CRenderer::OnResize(int InW, int InH)
{
    FlushCommandQueue(false);

    UINT64 FenceValue = GetCurrentFrameContext().FenceValue;

    for (UINT i = 0; i < TotalFrameCount; ++i)
    {
        PerFrameContext[i].FrameBuffer.Reset();
    }

    SwapChain->ResizeBuffers(TotalFrameCount, InW, InH, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

    D3D12_RENDER_TARGET_VIEW_DESC FrameBufferRtvDesc = {};
    FrameBufferRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // Apply gamma correction
    FrameBufferRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    for (UINT i = 0; i < TotalFrameCount; ++i)
    {
        SwapChain->GetBuffer(i, IID_PPV_ARGS(&(PerFrameContext[i].FrameBuffer)));
        D3dDevice->CreateRenderTargetView(PerFrameContext[i].FrameBuffer.Get(), &FrameBufferRtvDesc, PerFrameContext[i].FrameBufferRtvDescriptor);
    }

    CurrentFrameIndex = SwapChain->GetCurrentBackBufferIndex();
    GetCurrentFrameContext().FenceValue = (FenceValue + 1);

    ViewportWidth = InW;
    ViewportHeight = InH;

    Viewport.Width = static_cast<float>(ViewportWidth);
    Viewport.Height = static_cast<float>(ViewportHeight);

    ScissorRect = CD3DX12_RECT(0, 0, ViewportWidth, ViewportHeight);
    Scene->GetMainCamera()->SetAspectRatio(InW, InH);

    for (auto TextureIter = AllTextures.begin(); TextureIter != AllTextures.end(); TextureIter++)
    {
        TextureIter->second->OnResize(InW, InH);
    }
}