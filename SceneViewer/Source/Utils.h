#pragma once

#include "../stdafx.h"

using namespace DirectX;
using namespace Microsoft::WRL;

struct SSceneVertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 Tex;
};

struct SViewBuffer
{
	XMFLOAT4X4	ViewMatrix;
	XMFLOAT4X4	ProjectionMatrix;
	XMFLOAT4	CameraOrigin;
	XMFLOAT4	DirectionalLight;

	float padding[24];
};

struct SMeshInfo
{
	XMFLOAT4X4 WorldMatrix;
	int	TextureIdx;
};