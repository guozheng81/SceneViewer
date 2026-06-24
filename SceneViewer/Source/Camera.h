#pragma once

#include "Utils.h"

class CCamera
{
protected:
	float		AspectRatio = 1.778f;
	float		FOV = 0.3f;		// radians

	float		NearPlane = 1.0f;
	float		FarPlane = 10000.0f;

	float		Yaw = 0.0f;
	float		YawScale = 0.002f;

	float		Pitch = 0.0f;
	float		PitchScale = 0.002f;

	float		MoveSpeed = 2.5f;

	XMVECTOR	Position = { 0.0f, 0.0f, -10.0f };
	XMVECTOR	LookAtDirection = { 0.0f, 0.0f, 1.0f };
	XMVECTOR	RightDirection = { 1.0f, 0.0f, 0.0f };

public:
	void	SetPositionAndRotation(XMFLOAT3 InPos, float InYaw, float InPitch);

	void	SetAspectRatio(UINT InW, UINT InH);
	void	SetFOV(float InDegree);

	void	GetViewMatrix(XMFLOAT4X4* OutMtx);
	void	GetProjectionMatrix(XMFLOAT4X4* OutMtx);

	void	OnInputMouse(int InDeltaX, int InDeltaY);
	void	OnUpdate();
};

