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

void	CCamera::GetViewMatrix(XMFLOAT4X4* OutMtx)
{
	XMVECTOR Pos = XMVectorSet(Position.x, Position.y, Position.z, 1.0f);
	XMVECTOR Dir = XMVectorSet(LookAtDirection.x, LookAtDirection.y, LookAtDirection.z, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX Mtx = XMMatrixLookToLH(Pos, Dir, Up);
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(Mtx));
}

void	CCamera::GetProjectionMatrix(XMFLOAT4X4* OutMtx)
{
	XMMATRIX Mtx = XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(Mtx));
}