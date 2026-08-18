#include "Mesh.h"
#include "Renderer.h"

void CMesh::Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, bool bAlphaTest)
{
	bNeedsAlphaTest = bAlphaTest;

	VertexCount = Verts.size();
	UINT TotalSize = sizeof(SSceneVertex) * VertexCount;
	VertexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Verts.data(), TotalSize, VertexUploadBuffer);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = TotalSize;
	VertexBufferView.StrideInBytes = sizeof(SSceneVertex);

	IndicesCount = Indices.size();
	if (IndicesCount > 0)
	{
		TotalSize = sizeof(UINT32) * IndicesCount;
		IndexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Indices.data(), TotalSize, IndexUploadBuffer);
		IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = TotalSize;
	}
}

void CMesh::ResetUploadResource()
{
	if (VertexUploadBuffer)
	{
		VertexUploadBuffer.Reset();
	}

	if (IndexUploadBuffer)
	{
		IndexUploadBuffer.Reset();
	}

	BLAS_Scratch.Reset();
}

void CMesh::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	InCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	InCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	if (IndicesCount > 0)
	{
		InCommandList->IASetIndexBuffer(&IndexBufferView);
		InCommandList->DrawIndexedInstanced(IndicesCount, 1, 0, 0, 0);
	}
	else
	{
		InCommandList->DrawInstanced(VertexCount, 1, 0, 0);
	}
}

void	CMesh::GetWorldMatrix(XMFLOAT4X4* OutMtx)
{
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(WorldMatrix));
}

D3D12_GPU_VIRTUAL_ADDRESS CMesh::GetVertexGPUAddress()
{
	if (VertexBuffer)
	{
		return	VertexBuffer->GetGPUVirtualAddress();
	}
	return 0;
}

void CMesh::BuildBottomLevelAS(ID3D12GraphicsCommandList4* InCommandList)
{
	D3D12_RAYTRACING_GEOMETRY_DESC GeomDesc = {};

	GeomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	GeomDesc.Flags = (bNeedsAlphaTest? D3D12_RAYTRACING_GEOMETRY_FLAG_NONE : D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
	GeomDesc.Triangles.VertexBuffer.StartAddress = GetVertexGPUAddress();
	GeomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(SSceneVertex);
	GeomDesc.Triangles.VertexCount = VertexCount;
	GeomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BuildInputs = {};
	BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	BuildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	BuildInputs.NumDescs = 1;
	BuildInputs.pGeometryDescs = &GeomDesc;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PreBuildInfo = {};
	CRenderer::GetInstance().D3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BuildInputs, &PreBuildInfo);

	BLAS_Scratch.Init(PreBuildInfo.ScratchDataSizeInBytes, 1, false, D3D12_RESOURCE_STATE_COMMON, true);
	BLAS.Init(PreBuildInfo.ResultDataMaxSizeInBytes, 1, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = {};
	BuildDesc.Inputs = BuildInputs;
	BuildDesc.DestAccelerationStructureData = BLAS.GetGPUAddress();
	BuildDesc.ScratchAccelerationStructureData = BLAS_Scratch.GetGPUAddress();

	InCommandList->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS.GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);
}

void CMesh::CreateVertexShaderResourceView()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Buffer.FirstElement = 0;
	SrvDesc.Buffer.NumElements = VertexCount;
	SrvDesc.Buffer.StructureByteStride = sizeof(SSceneVertex);
	SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	SDescriptorHandle DescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
	CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(VertexBuffer.Get(), &SrvDesc, DescriptorHandle.CpuHandle);
	VertexSrvGPUDescriptor = DescriptorHandle.GpuHandle;
}