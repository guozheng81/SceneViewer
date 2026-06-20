#pragma once

#include "Utils.h"

class CTexture2D
{
public:
	ComPtr<ID3D12Resource> Texture;
	
	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;
	std::unique_ptr<uint8_t[]> DDSData;
};

class CMaterial
{
protected:
	ComPtr<ID3D12RootSignature>		RootSign;
	ComPtr<ID3D12PipelineState>		PSO;

public:
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

	CMaterial();

	void Build(LPCWSTR InVSFileName, LPCWSTR InPSFileName);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

