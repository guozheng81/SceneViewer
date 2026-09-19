#include "SceneObject.h"
#include "Mesh.h"

CSceneObject::CSceneObject(std::string ObjectName)
    : Name(std::move(ObjectName)), Parent(nullptr), bIsDirty(true) 
{
    XMStoreFloat3(&Position, XMVectorZero());
    XMStoreFloat3(&Rotation, XMVectorZero());
    XMStoreFloat3(&Scale, XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f));
    XMStoreFloat4x4(&WorldMatrix, XMMatrixIdentity());
}

CSceneObject::~CSceneObject() 
{

    for (auto* Mesh : Meshes)
    {
        Mesh->RemoveInstance(this);
    }
}

void CSceneObject::AddChild(CSceneObject* Child) 
{
    if (!Child) return;

    // If the child already has a parent, detach it first
    if (Child->Parent) 
    {
        Child->Parent->RemoveChild(Child);
    }

    Child->Parent = this;
    Child->Invalidate();
    Children.push_back(Child);
}

void CSceneObject::RemoveChild(CSceneObject* ChildToRemove) 
{
    for (auto It = Children.begin(); It != Children.end(); ++It) 
    {
        if (*It == ChildToRemove) {
            (*It)->Parent = nullptr;
            (*It)->Invalidate();
            Children.erase(It);
            return;
        }
    }
}

XMMATRIX CSceneObject::GetLocalMatrix() const 
{
    XMVECTOR Pos = XMLoadFloat3(&Position);
    XMVECTOR Scl = XMLoadFloat3(&Scale);

    float PitchRad = XMConvertToRadians(Rotation.x);
    float YawRad = XMConvertToRadians(Rotation.y);
    float RollRad = XMConvertToRadians(Rotation.z);
    XMVECTOR RotQuat = XMQuaternionRotationRollPitchYaw(PitchRad, YawRad, RollRad);

    return XMMatrixTransformation(
        XMVectorZero(), XMQuaternionIdentity(), Scl,
        XMVectorZero(), RotQuat, Pos
    );
}

XMMATRIX CSceneObject::GetWorldMatrix() 
{
    if (bIsDirty) 
    {
        UpdateWorldMatrix();
    }
    return XMLoadFloat4x4(&WorldMatrix);
}

void CSceneObject::Invalidate() 
{
    if (!bIsDirty) 
    {
        bIsDirty = true;
        for (auto* Child : Children) 
        {
            Child->Invalidate();
        }
    }
}

void CSceneObject::UpdateWorldMatrix() 
{
    XMMATRIX LocalMat = GetLocalMatrix();

    if (Parent) 
    {
        XMMATRIX ParentWorldMat = Parent->GetWorldMatrix();
        XMStoreFloat4x4(&WorldMatrix, XMMatrixMultiply(LocalMat, ParentWorldMat));
    }
    else 
    {
        XMStoreFloat4x4(&WorldMatrix, LocalMat);
    }

    bIsDirty = false;
}

void CSceneObject::AddMesh(CMesh* Mesh)
{
    if (!Mesh) return;

    // Check if mesh is already added
    for (const auto* ExistingMesh : Meshes)
    {
        if (ExistingMesh == Mesh) return;
    }

    if(Meshes.size() == 0)
    {
        // If this is the first mesh, set the bounding box to match the mesh's bounding box
        XMVECTOR MeshMin, MeshMax;
        Mesh->GetBoundingBox(MeshMin, MeshMax);
        BoundingBoxMin = MeshMin;
        BoundingBoxMax = MeshMax;
    }
    else
    {
        // Update the bounding box to encompass the new mesh's bounding box
        XMVECTOR MeshMin, MeshMax;
        Mesh->GetBoundingBox(MeshMin, MeshMax);
        BoundingBoxMin = XMVectorMin(BoundingBoxMin, MeshMin);
        BoundingBoxMax = XMVectorMax(BoundingBoxMax, MeshMax);
	}

    Meshes.push_back(Mesh);
	Mesh->AddInstance(this);
}

void CSceneObject::GetBoundingBox(XMVECTOR& OutMin, XMVECTOR& OutMax)
{
    // apply the world transformation to the local bounding box
    XMMATRIX WorldMat = GetWorldMatrix();

    XMVECTOR Corners[8] =
    {
        XMVectorSet(XMVectorGetX(BoundingBoxMin), XMVectorGetY(BoundingBoxMin), XMVectorGetZ(BoundingBoxMin), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMax), XMVectorGetY(BoundingBoxMin), XMVectorGetZ(BoundingBoxMin), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMin), XMVectorGetY(BoundingBoxMax), XMVectorGetZ(BoundingBoxMin), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMax), XMVectorGetY(BoundingBoxMax), XMVectorGetZ(BoundingBoxMin), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMin), XMVectorGetY(BoundingBoxMin), XMVectorGetZ(BoundingBoxMax), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMax), XMVectorGetY(BoundingBoxMin), XMVectorGetZ(BoundingBoxMax), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMin), XMVectorGetY(BoundingBoxMax), XMVectorGetZ(BoundingBoxMax), 1.0f),
        XMVectorSet(XMVectorGetX(BoundingBoxMax), XMVectorGetY(BoundingBoxMax), XMVectorGetZ(BoundingBoxMax), 1.0f)
    };

    XMVECTOR TransformedMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
    XMVECTOR TransformedMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

    for (const auto& Corner : Corners)
    {
        XMVECTOR TransformedCorner = XMVector3Transform(Corner, WorldMat);
        TransformedMin = XMVectorMin(TransformedMin, TransformedCorner);
        TransformedMax = XMVectorMax(TransformedMax, TransformedCorner);
    }

    OutMin = TransformedMin;
    OutMax = TransformedMax;
}