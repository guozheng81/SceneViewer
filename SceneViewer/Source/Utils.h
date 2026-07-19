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
	XMFLOAT4X4	ViewProjectionMatrix;
	XMFLOAT4X4	InvViewProjectionMatrix;
	XMFLOAT4X4	PrevViewProjectionMatrix;
	XMFLOAT4	CameraOrigin;
	XMFLOAT4	DirectionalLight;
	XMFLOAT4	ViewportSize;
	UINT		FrameNumber;

	float padding[3];
};

struct SMeshInfo
{
	XMFLOAT4X4 WorldMatrix;
	int	TextureIdx;
};