#pragma once

#include "Scene.h"

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
	virtual void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

class CLightPass : public CScreenPass
{
protected:
	CTexture2D* Albedo = nullptr;
	CTexture2D* Normal = nullptr;
	CTexture2D* Depth = nullptr;

public:
	virtual void Init();
	virtual void OnRender(ID3D12GraphicsCommandList* InCommandList);
};
