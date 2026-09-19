#pragma once

#include "Utils.h"

class CMesh;

class CSceneObject 
{
public:
    std::string Name;
    std::string FileName;

	XMFLOAT3 Albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);   // used when no albedo texture is present
	float Roughness = 0.6f; // used when no PRB texture is present
	float Metallic = 0.0f; // used when no PRB texture is present

    CSceneObject(std::string ObjectName);
    ~CSceneObject();

    // --- Transform Setters ---
    void SetPosition(const XMFLOAT3& InPos) { Position = InPos; Invalidate(); }
    void SetRotation(const XMFLOAT3& InEuler) { Rotation = InEuler; Invalidate(); }
    void SetScale(const XMFLOAT3& InScale) { Scale = InScale; Invalidate(); }

    const XMFLOAT3& GetLocalPosition() const { return Position; }
    const XMFLOAT3& GetLocalRotation() const { return Rotation; }
    const XMFLOAT3& GetLocalScale() const { return Scale; }

    void AddChild(CSceneObject* Child);
    void RemoveChild(CSceneObject* ChildToRemove);

    XMMATRIX GetLocalMatrix() const;
    XMMATRIX GetWorldMatrix();

    const std::vector<CSceneObject*>& GetChildren() const { return Children; }
    CSceneObject* GetParent() const { return Parent; }

    void AddMesh(CMesh* Mesh);
    const std::vector<CMesh*>& GetMeshes() const { return Meshes; }

    void GetBoundingBox(XMVECTOR& OutMin, XMVECTOR& OutMax);

private:
    XMFLOAT3 Position;
    XMFLOAT3 Rotation;
    XMFLOAT3 Scale;

    XMFLOAT4X4 WorldMatrix;
    bool bIsDirty = true;

    XMVECTOR BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
    XMVECTOR BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

    // Hierarchy pathways are entirely unowned raw pointers
    CSceneObject* Parent = nullptr;
    std::vector<CSceneObject*> Children;

    std::vector<CMesh*> Meshes;

    void Invalidate();
    void UpdateWorldMatrix();
};