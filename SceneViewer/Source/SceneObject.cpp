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
    if (Parent) 
    {
        Parent->RemoveChild(this);
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

    Meshes.push_back(Mesh);
	Mesh->AddInstance(this);
}