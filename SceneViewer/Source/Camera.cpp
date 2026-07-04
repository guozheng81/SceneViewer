#include "Camera.h"
#include "Renderer.h"

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
	Position = XMVectorSet(InPos.x, InPos.y, InPos.z, 1.0f);
	Yaw = InYaw;
	Pitch = InPitch;

	XMMATRIX RotMtx = XMMatrixRotationRollPitchYaw(Pitch, Yaw, 0.0f);
	RightDirection = RotMtx.r[0];
	LookAtDirection = RotMtx.r[2];
}

void	CCamera::GetCameraPosition(XMFLOAT4* OutPos)
{
	XMStoreFloat4(OutPos, Position);
}

void	CCamera::UpdateViewBuffer(SViewBuffer* OutViewBuffer)
{
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	ViewMatrix = XMMatrixLookToLH(Position, LookAtDirection, Up);
	ProjectionMatrix = XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);

	XMMATRIX ViewProjMtx = XMMatrixMultiply(ViewMatrix, ProjectionMatrix);
	XMMATRIX InvViewProjMtx = XMMatrixInverse(nullptr, ViewProjMtx);

	XMStoreFloat4x4(&(OutViewBuffer->ViewProjectionMatrix), XMMatrixTranspose(ViewProjMtx));
	XMStoreFloat4x4(&(OutViewBuffer->InvViewProjectionMatrix), XMMatrixTranspose(InvViewProjMtx));
}

void	CCamera::GetViewMatrix(XMFLOAT4X4* OutMtx)
{
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(ViewMatrix));
}

void	CCamera::GetProjectionMatrix(XMFLOAT4X4* OutMtx)
{
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(ProjectionMatrix));
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

	Position += LookAtDirection * (Z * MoveSpeed);
	Position += RightDirection * (X * MoveSpeed);
}