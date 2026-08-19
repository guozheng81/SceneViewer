#pragma once

#include "Utils.h"
#include "Renderer.h"
#include "Texture.h"


struct SRaytracingShaderInfo
{
	std::wstring	MissShader;

	std::wstring	HitGroup;
	std::wstring	ClosestHitShader;
	std::wstring	AnyHitShader;
};

class CMaterial
{
protected:
	ComPtr<ID3D12RootSignature>		RootSign;
	ComPtr<ID3D12PipelineState>		PSO;

	ComPtr<ID3D12StateObject>		RaytracingPSO;
	ComPtr<ID3D12StateObjectProperties> RtPSOProperties;

	std::vector<std::map<UINT, int>>	 SrvRegisterMap;	// textures and structured buffer
	std::vector<std::map<UINT, int>>	 ConstantRegisterMap;	//  constant buffer and constants
	std::vector<std::map<UINT, int>>	 UavRegisterMap;

	bool bUsedForRaytracing = false;
	CBuffer ShaderBindingTable;

	bool bUsedForCompute = false;

public:
	D3D12_DISPATCH_RAYS_DESC RaytraceDesc = {};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

	int FindSrvRootParameterIndex(UINT InRegister, UINT InSpace = 0);
	int FindConstantRootParameterIndex(UINT InRegister, UINT InSpace = 0);
	int FindUavRootParameterIndex(UINT InRegister, UINT InSpace = 0);

	CMaterial();

	// unbound srvs starting from space1
	static void IntRootParameters(UINT InCbvCount, UINT InSrvCount, UINT InUavCount, UINT InUnboundSrvCount, std::vector<CD3DX12_ROOT_PARAMETER>& RootParams, std::vector<CD3DX12_DESCRIPTOR_RANGE>& Ranges);
	void BuildRootSignature(std::vector<CD3DX12_ROOT_PARAMETER>& InRootParams, bool bInForRaytracing);
	void BuildPSO(LPCWSTR InVSFileName, LPCWSTR InPSFileName);

	void BuildRaytracingPSO(LPCWSTR InFileName, LPCWSTR InRayGenName, const std::vector<SRaytracingShaderInfo>& InShaderInfoArray, UINT MaxRecursionDepth = 1);

	void BuildComputePSO(LPCWSTR InComputeName);

	void OnRender(ID3D12GraphicsCommandList4* InCommandList);

	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);
	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer);

	void SetUav(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);

	void SetConstantBuffer(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer);

	void SetSceneForRaytracing(ID3D12GraphicsCommandList* InCommandList, class CScene* InScene);
};

