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
	CTexture2D* GBufferA = nullptr;
	CTexture2D* GBufferB = nullptr;
	CTexture2D* Depth = nullptr;

	CTexture2D* ShadowRT = nullptr;
	CTexture2D* IndirectLightRT = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CSimpleRTPass : public CScreenPass
{
protected:
	CTexture2D* SimpleRT = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CShadowRTPass : public CScreenPass
{
protected:
	CTexture2D* GBufferB = nullptr;
	CTexture2D* ShadowRT = nullptr;
	CTexture2D* Depth = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};

class CIndirectLightRTPass : public CScreenPass
{
protected:
	CTexture2D* GBufferB = nullptr;
	CTexture2D* IndirectLightRT = nullptr;
	CTexture2D* Depth = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList4* InCommandList);
};
