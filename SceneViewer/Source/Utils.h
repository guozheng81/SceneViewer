#pragma once

#include "../stdafx.h"

using namespace DirectX;
using namespace Microsoft::WRL;

struct SSceneVertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 Tex;
	XMFLOAT3 Tangent;
};

struct SViewBuffer
{
	XMFLOAT4X4	ViewMatrix;
	XMFLOAT4X4	ProjectionMatrix;

	float padding[32];
};