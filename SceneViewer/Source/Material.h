#pragma once

#include "Utils.h"

class CMaterial
{
protected:
	ComPtr<ID3D12RootSignature>		RootSign;
	ComPtr<ID3D12PipelineState>		PSO;

public:
	CMaterial(LPCWSTR InVSFileName, LPCWSTR InPSFileName, const D3D12_ROOT_SIGNATURE_DESC* InRootSignatureDesc);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

