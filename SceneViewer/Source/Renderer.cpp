#include "Renderer.h"

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
    SwapChainDesc.BufferCount = 3;
    SwapChainDesc.Width = ViewportWidth;
    SwapChainDesc.Height = ViewportHeight;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.SampleDesc.Count = 1;

    if (FAILED(DXgiFactory->CreateSwapChainForHwnd(D3DCommandQueue.Get(), hWnd, &SwapChainDesc, nullptr, nullptr, &SwapChain)))
    {
        return false;
    }

    DXgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    return true;
}
