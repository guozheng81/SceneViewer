#include "BarrierBatcher.h"
#include "Texture.h"
#include "Logger.h"
#include "Renderer.h"

#include <string>
#include <unordered_map>

namespace
{
    std::string ResourceStateToString(D3D12_RESOURCE_STATES InState)
    {
        static const std::unordered_map<int, const char*> StateNames =
        {
            { D3D12_RESOURCE_STATE_COMMON, "COMMON" },
            { D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, "VERTEX_AND_CONSTANT_BUFFER" },
            { D3D12_RESOURCE_STATE_INDEX_BUFFER, "INDEX_BUFFER" },
            { D3D12_RESOURCE_STATE_RENDER_TARGET, "RENDER_TARGET" },
            { D3D12_RESOURCE_STATE_UNORDERED_ACCESS, "UNORDERED_ACCESS" },
            { D3D12_RESOURCE_STATE_DEPTH_WRITE, "DEPTH_WRITE" },
            { D3D12_RESOURCE_STATE_DEPTH_READ, "DEPTH_READ" },
            { D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, "NON_PIXEL_SHADER_RESOURCE" },
            { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "PIXEL_SHADER_RESOURCE" },
            { D3D12_RESOURCE_STATE_STREAM_OUT, "STREAM_OUT" },
            { D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, "INDIRECT_ARGUMENT" },
            { D3D12_RESOURCE_STATE_COPY_DEST, "COPY_DEST" },
            { D3D12_RESOURCE_STATE_COPY_SOURCE, "COPY_SOURCE" },
            { D3D12_RESOURCE_STATE_RESOLVE_DEST, "RESOLVE_DEST" },
            { D3D12_RESOURCE_STATE_RESOLVE_SOURCE, "RESOLVE_SOURCE" },
            { D3D12_RESOURCE_STATE_GENERIC_READ, "GENERIC_READ" },
            { D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, "ALL_SHADER_RESOURCE" }
        };

        const auto Iter = StateNames.find(static_cast<int>(InState));
        if (Iter != StateNames.end())
        {
            return Iter->second;
        }

        return "UNKNOWN";
    }
}

void CBarrierBatcher::Request(CTexture* InTex, D3D12_RESOURCE_STATES InState)
{
    if (InTex == nullptr)
    {
        return;
    }

    const bool bAlreadyInState = (InTex->CurrentState == InState);

    for (auto Iter = Requests.begin(); Iter != Requests.end(); ++Iter)
    {
        if (Iter->Tex == InTex)
        {
            if (bAlreadyInState)
            {
                // the texture is already in the wanted state, so the pending request is obsolete
                Requests.erase(Iter);
            }
            else
            {
                Iter->State = InState;
            }
            return;
        }
    }

    if (bAlreadyInState)
    {
        return;
    }

    Requests.push_back({ InTex, InState });
}

void CBarrierBatcher::Flush(ID3D12GraphicsCommandList* InCommandList)
{
    std::vector<D3D12_RESOURCE_BARRIER> Barriers;
    Barriers.reserve(Requests.size());

    for (SRequest& Req : Requests)
    {
        if (Req.Tex->GetResource() == nullptr || Req.Tex->CurrentState == Req.State)
        {
            continue;
        }

        const D3D12_RESOURCE_STATES OldState = Req.Tex->CurrentState;
        Barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(Req.Tex->GetResource(), Req.Tex->CurrentState, Req.State));
        Req.Tex->CurrentState = Req.State;

		std::string TexName = CRenderer::GetInstance().GetTextureName(Req.Tex);
        LOG_INFO("BarrierBatcher: Transitioning %s : %s -> %s",
            TexName.c_str(), ResourceStateToString(OldState).c_str(), ResourceStateToString(Req.State).c_str());
    }

    Requests.clear();

    if (!Barriers.empty())
    {
        //InCommandList->ResourceBarrier(static_cast<UINT>(Barriers.size()), Barriers.data());
    }
}
