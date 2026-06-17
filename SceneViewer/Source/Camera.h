#pragma once

#include "Utils.h"

class CCamera
{
protected:
	float		AspectRatio = 1.778f;
	float		FOV = 0.3f;		// radians

	float		NearPlane = 0.1f;
	float		FarPlane = 1000.0f;

public:
	XMFLOAT3	Position = {0.0f, 0.0f, -10.0f};
	XMFLOAT3	LookAtDirection = {0.0f, 0.0f, 1.0f};

	void	SetAspectRatio(UINT InW, UINT InH);
	void	SetFOV(float InDegree);

	void	GetViewMatrix(XMFLOAT4X4* OutMtx);
	void	GetProjectionMatrix(XMFLOAT4X4* OutMtx);
};

