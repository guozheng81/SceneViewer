#pragma once

#include "Utils.h"
#include "Renderer.h"

class CBuffer;

class CTexture2D
{
protected:
	bool bNeedRtv;
	bool bNeedUav;
	bool bIsDepth;
	bool bIsDiffuse;

	XMFLOAT4 RTClearColor;

public:
	CTexture2D(bool InNeedRtv, bool InNeedUav, bool InIsDepth, bool InIsDiffuse);

	ComPtr<ID3D12Resource> Texture;

	inline ID3D12Resource* GetResource() {
		return Texture.Get();
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCPUDescriptor = {};

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCPUDescriptor = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE DsvCPUDescriptor = {};

	CD3DX12_CPU_DESCRIPTOR_HANDLE UavCPUDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE UavGPUDescriptor = {};

	UINT		Width = 0;
	UINT		Height = 0;
	
	// keep alive till gpu finish
	ComPtr<ID3D12Resource> UploadTexture;
	std::unique_ptr<uint8_t[]> DDSData;

	void ResetUploadResource();

	void CreateShaderResourceView(bool bIsResizing = false);
	void CreateRenderTargetView(bool bIsResizing = false);
	void CreateUnorderedAccessView(bool bIsResizing = false);

	void CreateDepthTextureResource();
	void CreateRenderTargetResource(DXGI_FORMAT InFormat, XMFLOAT4 InColor);

	void OnResize(UINT InW, UINT InH);
};

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

	void BuildRaytracingPSO(LPCWSTR InFileName, LPCWSTR InRayGenName, const std::vector<SRaytracingShaderInfo>& InShaderInfoArray);

	void OnRender(ID3D12GraphicsCommandList4* InCommandList);

	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);
	void SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer);

	void SetUav(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex);

	void SetConstantBuffer(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer);

	void SetSceneForRaytracing(ID3D12GraphicsCommandList* InCommandList, class CScene* InScene);
};

