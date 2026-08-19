#pragma once

#include "Utils.h"

class CTexture
{
public:
	ComPtr<ID3D12Resource> Texture;

	CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCPUDescriptor = {};

	inline ID3D12Resource* GetResource() {
		return Texture.Get();
	}
};

class CTexture2D : public CTexture
{
protected:
	bool bNeedRtv;
	bool bNeedUav;
	bool bIsDepth;
	bool bIsDiffuse;

	XMFLOAT4 RTClearColor;

public:
	CTexture2D(bool InNeedRtv, bool InNeedUav, bool InIsDepth, bool InIsDiffuse);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE DsvCPUDescriptor = {};

	CD3DX12_CPU_DESCRIPTOR_HANDLE UavCPUDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE UavGPUDescriptor = {};

	UINT		Width = 0;
	UINT		Height = 0;

	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;

	void Init(LPCWSTR InFileName, ID3D12GraphicsCommandList4* InCommandList);

	void ResetUploadResource();

	void CreateShaderResourceView(bool bIsResizing = false);
	void CreateRenderTargetView(bool bIsResizing = false);
	void CreateUnorderedAccessView(bool bIsResizing = false);

	void CreateDepthTextureResource();
	void CreateRenderTargetResource(DXGI_FORMAT InFormat, XMFLOAT4 InColor);

	void OnResize(UINT InW, UINT InH);
};


