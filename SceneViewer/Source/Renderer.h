#pragma once

#include "Utils.h"


class CRenderer
{
public:
	UINT	ViewportWidth = 1280;
	UINT	ViewportHeight = 720;

	ComPtr<ID3D12Device>	D3dDevice;
	ComPtr<ID3D12CommandQueue> D3DCommandQueue;
	ComPtr<IDXGISwapChain1>	SwapChain;

	bool	Init(HWND hWnd);
	void	Render() {};
	void	Shutdown() {};

};

