#pragma once

#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"

class CScreenPass
{
protected:
	CMesh*		ScreenQuad = nullptr;
	CMaterial	Material;

public:
	virtual ~CScreenPass()
	{
	}

	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CLightPass : public CScreenPass
{
protected:
	CTextureRenderTarget* GBufferA = nullptr;
	CTextureRenderTarget* GBufferB = nullptr;
	CTextureDepthStencil* Depth = nullptr;

	CTextureRenderTarget* ShadowRT = nullptr;
	CTextureRenderTarget* IndirectLightRT = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CSimpleRTPass : public CScreenPass
{
protected:
	CTextureRenderTarget* SimpleRT = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CShadowRTPass : public CScreenPass
{
protected:
	CTextureRenderTarget* GBufferB = nullptr;
	CTextureRenderTarget* ShadowRT = nullptr;
	CTextureDepthStencil* Depth = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

struct SATrousConstants
{
	int	g_StepSize = 1;
	float g_PhiColor = 20.0f;
	float g_PhiNormal = 3.0f;
	float g_PhiDepth = 0.05f;
	float Proj_m32;
	float Proj_m22;
	UINT ViewportWidth;
	UINT ViewportHeight;
};

class CIndirectLightRTPass : public CScreenPass
{
protected:
	CTextureRenderTarget* GBufferB = nullptr;
	CTextureRenderTarget* IndirectLightRT = nullptr;
	CTextureDepthStencil* Depth = nullptr;

	CMaterial TemporalAccumulate;
	CTextureRenderTarget* TA0 = nullptr;
	CTextureRenderTarget* TA1 = nullptr;
	bool bIsTA1Target = true;

	SATrousConstants ATrousConstants;
	CMaterial	ATrousMaterial;
	CTextureRenderTarget* ATrous0 = nullptr;
	CTextureRenderTarget* ATrous1 = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};
