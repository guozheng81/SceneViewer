#pragma once

#include "Utils.h"

class CTexture;

class CBarrierBatcher
{
private:
    struct SRequest
    {
        CTexture* Tex = nullptr;
        D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
    };

    std::vector<SRequest> Requests;

public:
    // Records the state a texture must be in at the next flush. A later request for the same texture overrides it.
    void Request(CTexture* InTex, D3D12_RESOURCE_STATES InState);

    // Emits one batched ResourceBarrier call for all textures whose tracked state differs from the requested one
    void Flush(ID3D12GraphicsCommandList* InCommandList);
};
