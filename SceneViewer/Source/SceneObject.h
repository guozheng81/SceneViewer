#pragma once

#include "Utils.h"

class CSceneObject 
{
public:
    std::string Name;

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

private:
    XMFLOAT3 Position;
    XMFLOAT3 Rotation;
    XMFLOAT3 Scale;

    XMFLOAT4X4 WorldMatrix;
    bool bIsDirty = true;

    // Hierarchy pathways are entirely unowned raw pointers
    CSceneObject* Parent = nullptr;
    std::vector<CSceneObject*> Children;

    void Invalidate();
    void UpdateWorldMatrix();
};