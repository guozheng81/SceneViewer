#pragma once

#include "Utils.h"

class CTexture
{
protected:
	UINT		Width = 0;
	UINT		Height = 0;

	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT SrvFormat = DXGI_FORMAT_UNKNOWN;

public:
	CTexture() = default;
	CTexture(DXGI_FORMAT InFormat, UINT InW, UINT InH) : Width(InW), Height(InH), Format(InFormat), SrvFormat(InFormat) {}
	virtual ~CTexture() = default;

	CTexture(const CTexture&) = delete;
	CTexture& operator=(const CTexture&) = delete;
	CTexture(CTexture&&) noexcept = default;
	CTexture& operator=(CTexture&&) noexcept = default;

	ComPtr<ID3D12Resource> Texture;

	CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCPUDescriptor = {};

	inline ID3D12Resource* GetResource() {
		return Texture.Get();
	}

	virtual void CreateResource() {}
	virtual void ResetUploadResource() {}

	virtual void CreateShaderResourceView(bool bIsResizing = false);
	virtual void OnResize(UINT InW, UINT InH) {
		Width = InW;
		Height = InH;
	}
};

class CTexture2D : public CTexture
{
protected:
	bool bIsDiffuse;

	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;

public:
	CTexture2D(bool InIsDiffuse);

	void LoadResource(LPCWSTR InFileName, ID3D12GraphicsCommandList4* InCommandList);

	virtual void ResetUploadResource();

};

class CTextureRenderTarget : public CTexture
{
private:
	bool bNeedRtv;
	bool bNeedUav;
	XMFLOAT4 RTClearColor;

public:

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCPUDescriptor = {};

	CD3DX12_CPU_DESCRIPTOR_HANDLE UavCPUDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE UavGPUDescriptor = {};

	CTextureRenderTarget(DXGI_FORMAT InFormat, XMFLOAT4 InColor, UINT InW, UINT InH, bool InNeedRtv, bool InNeedUav);

	virtual void CreateResource();

	void CreateRenderTargetView(bool bIsResizing = false);
	void CreateUnorderedAccessView(bool bIsResizing = false);

	virtual void OnResize(UINT InW, UINT InH);
};

class CTextureDepthStencil : public CTexture
{
private:

public:
	CD3DX12_CPU_DESCRIPTOR_HANDLE DsvCPUDescriptor = {};

	CTextureDepthStencil(DXGI_FORMAT InFormat, UINT InW, UINT InH);

	virtual void CreateResource();
	void CreateDepthStencilView(bool bIsResizing = false);

	virtual void OnResize(UINT InW, UINT InH);
};
