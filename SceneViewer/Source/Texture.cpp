#include "Texture.h"
#include "Renderer.h"
#include "DDSTextureLoader12.h"
#include "Logger.h"

DXGI_FORMAT ConvertUnormToSrgb(DXGI_FORMAT format)
{
    switch (format)
    {
        // Standard formats
    case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8X8_UNORM: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

        // Block Compressed (BC) formats
    case DXGI_FORMAT_BC1_UNORM:      return DXGI_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:      return DXGI_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:      return DXGI_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM:      return DXGI_FORMAT_BC7_UNORM_SRGB;

        // Already SRGB or does not support hardware gamma mapping
    default:                         return format;
    }
}

void CTexture::CreateShaderResourceView(bool bIsResizing)
{
    if(Texture == nullptr)
    {
        LOG_ERROR("Texture resource is null. Cannot create Shader Resource View.");
        return;
	}

    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = SrvFormat;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = Texture->GetDesc().MipLevels;
    SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    if (!bIsResizing)
    {
        SDescriptorHandle SrvDescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
        SrvCPUDescriptor = SrvDescriptorHandle.CpuHandle;
        SrvGPUDescriptor = SrvDescriptorHandle.GpuHandle;
    }

    CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(Texture.Get(), &SrvDesc, SrvCPUDescriptor);
}

CTexture2D::CTexture2D(bool InIsDiffuse)
    : bIsDiffuse(InIsDiffuse)
{
}

void CTexture2D::LoadResource(LPCWSTR InFileName, ID3D12GraphicsCommandList4* InCommandList)
{
    std::vector<D3D12_SUBRESOURCE_DATA> Subresources;
    std::unique_ptr<uint8_t[]> DDSData;

    if (FAILED(LoadDDSTextureFromFile(CRenderer::GetInstance().D3dDevice.Get(), InFileName, Texture.GetAddressOf(), DDSData, Subresources)))
    {
		LOG_ERROR("Failed to load texture from file: %ls", InFileName);
        return;
    }

    UINT64 ReqSize = GetRequiredIntermediateSize(GetResource(), 0, static_cast<UINT>(Subresources.size()));

    CD3DX12_HEAP_PROPERTIES UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC ResDesc = CD3DX12_RESOURCE_DESC::Buffer(ReqSize);

    HRESULT hr = CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(UploadTexture.GetAddressOf()));
    if (FAILED(hr))
    {
        Texture.Reset();  // Clean up texture on failure
		LOG_ERROR("Failed to create upload resource for texture: %ls", InFileName);
        return;
    }
    UpdateSubresources(InCommandList, GetResource(), UploadTexture.Get(), 0, 0, static_cast<UINT>(Subresources.size()), Subresources.data());
    CRenderer::GetInstance().ResourceBarrier(GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Width = Texture->GetDesc().Width;
    Height = Texture->GetDesc().Height;
	Format = Texture->GetDesc().Format;

    if (bIsDiffuse)
    {
        SrvFormat = ConvertUnormToSrgb(Format);
    }
    else
    {
		SrvFormat = Format;
    }
}

void CTexture2D::ResetUploadResource()
{
    if (UploadTexture)
    {
        UploadTexture.Reset();
    }
}

void CTextureDepthStencil::OnResize(UINT InW, UINT InH)
{
    Texture.Reset();
    Width = InW;
    Height = InH;

    CreateResource();

	CreateDepthStencilView(true);
    CreateShaderResourceView(true);
}

void CTextureDepthStencil::CreateDepthStencilView(bool bIsResizing)
{
    if (Texture == nullptr)
    {
        LOG_ERROR("Texture resource is null. Cannot create Depth Stencil View.");
        return;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
    DsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // Cast from R32_TYPELESS
    DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    DsvDesc.Texture2D.MipSlice = 0;

    if (!bIsResizing)
    {
        DsvCPUDescriptor = CRenderer::GetInstance().DsvDescriptorAllocator.Allocate().CpuHandle;
    }
    CRenderer::GetInstance().D3dDevice->CreateDepthStencilView(GetResource(), &DsvDesc, DsvCPUDescriptor);
}

CTextureDepthStencil::CTextureDepthStencil(DXGI_FORMAT InFormat, UINT InW, UINT InH)
    : CTexture(InFormat, InW, InH)
{
	SrvFormat = DXGI_FORMAT_R32_FLOAT; // Use a compatible format for shader resource view
}

void CTextureDepthStencil::CreateResource()
{
    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = Format; // Use typeless so it can be cast to DSV and SRV
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
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
    HRESULT hr = CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &ClearValue, IID_PPV_ARGS(Texture.GetAddressOf()));
    if (FAILED(hr))
    {
		LOG_ERROR("Failed to create depth stencil resource.");
    }
}

CTextureRenderTarget::CTextureRenderTarget(DXGI_FORMAT InFormat, XMFLOAT4 InColor, UINT InW, UINT InH, bool InNeedRtv, bool InNeedUav)
	: CTexture(InFormat, InW, InH),
	RTClearColor(InColor),
    bNeedRtv(InNeedRtv),
	bNeedUav(InNeedUav)
{
}

void CTextureRenderTarget::CreateResource()
{
    D3D12_RESOURCE_DESC TextureDesc = {};

    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TextureDesc.Alignment = 0;
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = Format;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
    if (bNeedRtv)
    {
        Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (bNeedUav)
    {
        Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    TextureDesc.Flags = Flags;

    D3D12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = TextureDesc.Format;
    ClearValue.Color[0] = RTClearColor.x;
    ClearValue.Color[1] = RTClearColor.y;
    ClearValue.Color[2] = RTClearColor.z;
    ClearValue.Color[3] = RTClearColor.w;

    HRESULT hr = CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_COMMON, (bNeedRtv ? &ClearValue : nullptr), IID_PPV_ARGS(Texture.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create render target resource.");
		return;
    }
}

void CTextureRenderTarget::CreateRenderTargetView(bool bIsResizing)
{
    if (Texture == nullptr)
    {
		LOG_ERROR("Texture resource is null. Cannot create Render Target View.");
        return;
    }

    if (!bIsResizing)
    {
        RtvCPUDescriptor = CRenderer::GetInstance().RtvDescriptorAllocator.Allocate().CpuHandle;
    }
    CRenderer::GetInstance().D3dDevice->CreateRenderTargetView(Texture.Get(), nullptr, RtvCPUDescriptor);
}

void CTextureRenderTarget::CreateUnorderedAccessView(bool bIsResizing)
{
    if(Texture == nullptr)
    {
        LOG_ERROR("Texture resource is null. Cannot create Unordered Access View.");
        return;
	}

    if (!bIsResizing)
    {
        SDescriptorHandle DescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
        UavCPUDescriptor = DescriptorHandle.CpuHandle;
        UavGPUDescriptor = DescriptorHandle.GpuHandle;
    }

    CRenderer::GetInstance().D3dDevice->CreateUnorderedAccessView(Texture.Get(), nullptr, nullptr, UavCPUDescriptor);
}

void CTextureRenderTarget::OnResize(UINT InW, UINT InH)
{
    Texture.Reset();
    Width = InW;
    Height = InH;

    CreateResource();
    CreateShaderResourceView(true);

    if (bNeedRtv)
    {
        CreateRenderTargetView(true);
    }
    if (bNeedUav)
    {
        CreateUnorderedAccessView(true);
    }
}

