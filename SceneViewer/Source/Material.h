#pragma once

#include "Utils.h"

class CUniformBuffer;

class CTexture2D
{
protected:
	int		SrvDescriptorIndex = -1;

public:
	ComPtr<ID3D12Resource> Texture;

	UINT		Width = 0;
	UINT		Height = 0;
	
	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;
	std::unique_ptr<uint8_t[]> DDSData;

	void CreateShaderResourceView();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrvGPUDescriptor();
	inline	bool HasValidSrv() const {
		return SrvDescriptorIndex >= 0;
	}
};

class CMaterial
{
protected:
	ComPtr<ID3D12RootSignature>		RootSign;
	ComPtr<ID3D12PipelineState>		PSO;

	int FindSrvRootParameterIndex(UINT InRegister);

public:
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

	CMaterial();

	void Build(LPCWSTR InVSFileName, LPCWSTR InPSFileName);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);

	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);
	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CUniformBuffer* InBuffer);
};

