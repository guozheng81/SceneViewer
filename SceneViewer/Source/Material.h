#pragma once

#include "Utils.h"

class CUniformBuffer;

class CTexture2D
{
protected:

public:
	ComPtr<ID3D12Resource> Texture;

	inline ID3D12Resource* GetResource() {
		return Texture.Get();
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCPUDescriptor = {};

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCPUDescriptor = {};

	UINT		Width = 0;
	UINT		Height = 0;
	
	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;
	std::unique_ptr<uint8_t[]> DDSData;

	void ResetUploadResource();

	void CreateShaderResourceView();

	void CreateRenderTargetView();
};

class CMaterial
{
protected:
	ComPtr<ID3D12RootSignature>		RootSign;
	ComPtr<ID3D12PipelineState>		PSO;

	std::vector<CD3DX12_DESCRIPTOR_RANGE> SrvRangeArray;
	int FindSrvRootParameterIndex(UINT InRegister);

public:
	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;

	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

	CMaterial();

	void IntRootParameters(UINT InCbvCount, UINT InSrvCount, UINT InRtvCount);
	void Build(LPCWSTR InVSFileName, LPCWSTR InPSFileName);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);

	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);
	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CUniformBuffer* InBuffer);

	void SetConstantBuffer(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CUniformBuffer* InBuffer);
};

