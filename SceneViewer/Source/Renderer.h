#pragma once

#include "Utils.h"

class CScene;

class CUniformBuffer
{
protected:
	BYTE* MappedPtr = nullptr;

	UINT ElementSize = 0;
	UINT ElementCount = 0;

public:
	ComPtr<ID3D12Resource> Buffer;

	CUniformBuffer(UINT InEleSize, UINT InEleCount);
	~CUniformBuffer();

	void SetData(void* InData);
};

struct SPerFrameContext
{
	ComPtr<ID3D12CommandAllocator>	CommandAllocator;

	ComPtr<ID3D12Resource>	FrameBuffer;

	std::unique_ptr<CUniformBuffer> ViewBuffer;

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

	inline SPerFrameContext& GetCurrentFrameContext()
	{
		return PerFrameContext[CurrentFrameIndex];
	}

	ComPtr<ID3D12GraphicsCommandList>	CommandList;

	void	BeginFrame();
	void	EndFrame();

	CD3DX12_VIEWPORT Viewport;
	CD3DX12_RECT ScissorRect;

	SViewBuffer	 ViewBuffer;

	ComPtr<ID3D12DescriptorHeap>	RtvDescriptorHeap;
	UINT	RtvDescriptorSize = 0;

	ComPtr<ID3D12DescriptorHeap>	SrvDescriptorHeap;
	UINT	SrvDescriptorSize = 0;

	void	FlushCommandQueue();

	std::unique_ptr<CScene>	Scene;

public:
	UINT	ViewportWidth = 1280;
	UINT	ViewportHeight = 720;

	ComPtr<ID3D12Device>	D3dDevice;
	ComPtr<ID3D12CommandQueue> D3DCommandQueue;

	static CRenderer& GetInstance();

	bool	Init(HWND hWnd);
	void	Render();
	void	Shutdown();

	CScene* GetScene()
	{
		return Scene.get();
	}

	void	LoadScene();

	void	UpdateViewBuffer();

	void ResourceBarrier(ID3D12Resource* InResource, D3D12_RESOURCE_STATES InBefore, D3D12_RESOURCE_STATES InAfter);

	ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* InData, UINT InTotalByteSize, ComPtr<ID3D12Resource>& OutUploadBuffer);
};

