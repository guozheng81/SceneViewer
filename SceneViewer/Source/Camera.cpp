#include "Camera.h"

void	CCamera::SetAspectRatio(UINT InW, UINT InH)
{
	if (InH != 0)
	{
		AspectRatio = (float)InW / (float)InH;
	}
}

void	CCamera::SetFOV(float InDegree)
{
	FOV = XMConvertToRadians(InDegree);
}

void	CCamera::SetPositionAndRotation(XMFLOAT3 InPos, float InYaw, float InPitch)
{
	Position = InPos;
	Yaw = InYaw;
	Pitch = InPitch;

	XMMATRIX RotMtx = XMMatrixRotationRollPitchYaw(Pitch, Yaw, 0.0f);
	RightDirection = RotMtx.r[0];
	LookAtDirection = RotMtx.r[2];
}

void	CCamera::GetViewMatrix(XMFLOAT4X4* OutMtx)
{
	XMVECTOR Pos = XMVectorSet(Position.x, Position.y, Position.z, 1.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX Mtx = XMMatrixLookToLH(Pos, LookAtDirection, Up);
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(Mtx));
}

void	CCamera::GetProjectionMatrix(XMFLOAT4X4* OutMtx)
{
	XMMATRIX Mtx = XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(Mtx));
}

void	CCamera::OnInputMouse(int InDeltaX, int InDeltaY)
{
	Yaw += InDeltaX * YawScale;
	Pitch += InDeltaY * PitchScale;

	XMMATRIX RotMtx = XMMatrixRotationRollPitchYaw(Pitch, Yaw, 0.0f);
	RightDirection = RotMtx.r[0];
	LookAtDirection = RotMtx.r[2];
}

void	CCamera::OnUpdate()
{
	int X = 0;
	int Z = 0;
	if (GetAsyncKeyState('W') & 0x8000)	
	{
		Z += 1;
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		Z -= 1;
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		X -= 1;
	}

	if (GetAsyncKeyState('D') & 0x8000)
	{
		X += 1;
	}

	XMVECTOR Pos = XMVectorSet(Position.x, Position.y, Position.z, 1.0f);
	Pos += LookAtDirection * (Z * MoveSpeed);
	Pos += RightDirection * (X * MoveSpeed);

	XMStoreFloat3(&Position, Pos);
}