#include "stdafx.h"
#include "./Source/Renderer.h"
#include "./Source/Scene.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

HWND        g_HWnd = nullptr;
int         g_LastMousePositionX = 0;
int         g_LastMousePositionY = 0;
bool        g_InSizeMove = false;

float       g_DirLightX = -0.3f;
float       g_DirLightZ = -0.15f;
float       g_DirLightInensity = 5.0f;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_CREATE:
    {
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        return 0;

    case WM_KEYUP:
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        g_LastMousePositionX = (short)LOWORD(lParam);
        g_LastMousePositionY = (short)HIWORD(lParam);
        SetCapture(g_HWnd);
    }
    return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
    {
        int X = (short)LOWORD(lParam);
        int Y = (short)HIWORD(lParam);

        if (MK_LBUTTON & wParam)
        {
            CScene* Scene = CRenderer::GetInstance().GetScene();
            if (Scene)
            {
                Scene->GetMainCamera()->OnInputMouse(X - g_LastMousePositionX, Y - g_LastMousePositionY);
            }
        }
        else if (MK_RBUTTON & wParam)
        {
            CScene* Scene = CRenderer::GetInstance().GetScene();
            if (Scene)
            {
                XMFLOAT3 Right, Forward;
                Scene->GetMainCamera()->GetAxesXZ(&Right, &Forward, true);

                float DeltaX = (X - g_LastMousePositionX) * 0.005f;
                float DeltaY = (Y - g_LastMousePositionY) * 0.005f;

                // Drag right -> light moves toward camera right; drag down -> light moves toward the camera
                g_DirLightX += Right.x * DeltaX - Forward.x * DeltaY;
                g_DirLightZ += Right.z * DeltaX - Forward.z * DeltaY;

                const float DIR_LIGHT_MAX_RADIUS = 1.5f;
                float LightLength = sqrtf(g_DirLightX * g_DirLightX + g_DirLightZ * g_DirLightZ);
                if (LightLength > DIR_LIGHT_MAX_RADIUS)
                {
                    float Scale = DIR_LIGHT_MAX_RADIUS / LightLength;
                    g_DirLightX *= Scale;
                    g_DirLightZ *= Scale;
                }

                Scene->SetDirectionalLight(XMFLOAT3(g_DirLightX, -1.0f, g_DirLightZ), g_DirLightInensity);
            }
        }

        g_LastMousePositionX = X;
        g_LastMousePositionY = Y;
    }
    return 0;
    case WM_SIZE:
    {
        int Width = LOWORD(lParam);
        int Height = HIWORD(lParam);

        // Don't resize DX12 resources if the window is minimized
        if (wParam == SIZE_MINIMIZED)
        {
            break;
        }

        // Only resize instantly if the user clicked Maximize/Restore, 
        // or if the window size changed via code (not dragging)
        if (!g_InSizeMove)
        {
            CRenderer::GetInstance().OnResize(Width, Height);
        }
    }
    return 0;

    case WM_ENTERSIZEMOVE:
        g_InSizeMove = true;
        return 0;

    case WM_EXITSIZEMOVE:
    {
        g_InSizeMove = false;

        // Trigger the DirectX 12 resize now that resizing has stopped
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int Width = clientRect.right - clientRect.left;
        int Height = clientRect.bottom - clientRect.top;

        CRenderer::GetInstance().OnResize(Width, Height);
    }
    return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"SceneViewerClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, CRenderer::GetInstance().ViewportWidth, CRenderer::GetInstance().ViewportHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    g_HWnd = CreateWindow(
        windowClass.lpszClassName,
        L"SceneViewer_DX12",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark(); // Or StyleColorsLight()
    ImGui_ImplWin32_Init(g_HWnd);

    CRenderer::GetInstance().Init(g_HWnd);

    CScene* Scene = CRenderer::GetInstance().GetScene();
    if (Scene)
    {
        Scene->SetDirectionalLight(XMFLOAT3(g_DirLightX, -1.0f, g_DirLightZ), g_DirLightInensity);
    }

    ShowWindow(g_HWnd, nCmdShow);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
			CRenderer::GetInstance().Render();
        }
    }

    CRenderer::GetInstance().Shutdown();

    return static_cast<char>(msg.wParam);
}
