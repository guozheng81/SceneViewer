#pragma once

#include "Utils.h"
#include <filesystem>

class CScene;
class CTexture2D;
class CScreenPass;
class CMesh;

class CBuffer
{
protected:
	ComPtr<ID3D12Resource> Buffer;
	BYTE* MappedPtr = nullptr;

	UINT ElementSize = 0;
	UINT ElementCount = 0;

	bool bUseForUpload = false;

public:
	CBuffer();
	~CBuffer();

	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCPUDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGPUDescriptor = {};

	void Init(UINT InEleSize, UINT InEleCount, bool InForUpload, D3D12_RESOURCE_STATES InInitState = D3D12_RESOURCE_STATE_GENERIC_READ, bool bNeedUAV = false);
	inline ID3D12Resource* GetResource() {
		return Buffer.Get();
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress();

	void CreateShaderResourceView();

	void SetData(void* InData);
};

struct SPerFrameContext
{
	ComPtr<ID3D12CommandAllocator>	CommandAllocator;

	ComPtr<ID3D12Resource>	FrameBuffer;
	CD3DX12_CPU_DESCRIPTOR_HANDLE FrameBufferRtvDescriptor = {};

	CBuffer ViewBuffer;

	UINT64 FenceValue = 0;
};

class CRenderer
{
protected:
	ComPtr<IDXGISwapChain3>	SwapChain;

	ComPtr<ID3D12Fence>		FrameFence;
	HANDLE					FrameFenceEvent;

	UINT	CurrentFrameIndex = 0;
	const static UINT	TotalFrameCount = 3;

	SPerFrameContext	PerFrameContext[TotalFrameCount];

	ComPtr<ID3D12GraphicsCommandList4>	CommandList;

	void	BeginFrame();
	void	EndFrame();

	CD3DX12_VIEWPORT Viewport;
	CD3DX12_RECT ScissorRect;

	SViewBuffer	 ViewBuffer;

	ComPtr<ID3D12DescriptorHeap>	RtvDescriptorHeap;
	UINT	RtvDescriptorSize = 0;
	int		CurrentRtvDescriptorIndex = 0;

	ComPtr<ID3D12DescriptorHeap>	DsvDescriptorHeap;
	UINT	DsvDescriptorSize = 0;

	ComPtr<ID3D12DescriptorHeap>	SrvDescriptorHeap;
	UINT	SrvDescriptorSize = 0;
	int		CurrentSrvDescriptorIndex = 0;

	void	FlushCommandQueue(bool bShouldIncreaseFence = true);

	std::unique_ptr<CScene>	Scene;

	std::map<std::string, std::unique_ptr<CTexture2D>>  AllTextures;

	std::unique_ptr<CMesh>	ScreenQuad;
	std::vector<std::unique_ptr<CScreenPass>>	ScreenPasses;

public:
	UINT	ViewportWidth = 1280;
	UINT	ViewportHeight = 720;

	ComPtr<ID3D12Device5>	D3dDevice;
	ComPtr<ID3D12CommandQueue> D3DCommandQueue;

	std::vector<CD3DX12_STATIC_SAMPLER_DESC> TextureSamplers;

	inline SPerFrameContext& GetCurrentFrameContext()
	{
		return PerFrameContext[CurrentFrameIndex];
	}

	static CRenderer& GetInstance();

	bool	Init(HWND hWnd);
	void	Render();
	void	Shutdown();

	inline CScene* GetScene()	{		return Scene.get();	}
	inline CMesh* GetScreenQuad() {	return ScreenQuad.get();	}

	void	LoadScene();

	void	UpdateViewBuffer();

	void ResourceBarrier(ID3D12Resource* InResource, D3D12_RESOURCE_STATES InBefore, D3D12_RESOURCE_STATES InAfter);

	ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* InData, UINT InTotalByteSize, ComPtr<ID3D12Resource>& OutUploadBuffer);

	static std::filesystem::path GetExeDirectory();
	static std::filesystem::path GetAssetDirectory();

	CTexture2D* LoadTexture(const std::string& InFileName, bool InIsDiffuse = false);
	CTexture2D* GetTexture(const std::string& InFileName);
	CTexture2D* CreateDepthTexture(const std::string& InName, UINT InW, UINT InH);
	CTexture2D* CreateRenderTarget(const std::string& InName, DXGI_FORMAT InFormat, XMFLOAT4 InColor, UINT InW = 0, UINT InH = 0);

	inline int GetCurrentSrvDescriptorIndex() const {	return CurrentSrvDescriptorIndex;	}
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrvGPUDescriptor(UINT Idx);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocSrvDescriptor(int& OutDescriptorIdx);
	int	GetSrvDescriptorOffset(CD3DX12_GPU_DESCRIPTOR_HANDLE InStart, CD3DX12_GPU_DESCRIPTOR_HANDLE InEnd);

	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocRtvDescriptor(int& OutDescriptorIdx);

	void	OnResize(int InW, int InH);
};

