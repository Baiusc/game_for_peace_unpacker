// UCF 透明叠加层（Windows）：Win32 分层窗口 + D3D11 + ImGui。
// 窗口风格 WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST：
//   - LAYERED + 透明 swap chain 清空色 (0,0,0,0) -> 只显示 ImGui 画的内容；
//   - TRANSPARENT -> 整窗不吃鼠标（点击穿透到下面的游戏）；
//   - TOPMOST -> 永远盖在游戏之上。
// 数据来自 SharedTransport（Python 宿主写入）；没有共享内存时退回 SyntheticSource 自演。
//
// 仅在 WIN32 下编译/运行。Linux 上这个文件不参与构建（见 CMakeLists.txt）。
#include "overlay_win32.hpp"
#include "menu_win32.hpp"
#include "config.hpp"
#include "shared_state.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>           // CreateDXGIFactory2
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

// imgui_impl_win32.h 把 ImGui_ImplWin32_WndProcHandler 的声明放在了 #if 0 中，
// 按官方示例需在此自行前向声明后调用。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device*            g_device = nullptr;
static ID3D11DeviceContext*     g_ctx    = nullptr;
static IDXGISwapChain1*         g_swap   = nullptr;
static ID3D11RenderTargetView*  g_rtv    = nullptr;
static HWND                     g_hwnd   = nullptr;
static ucf::Settings            g_settings{};
static bool                     g_show_menu = true;
static ucf::SyntheticSource     g_synth{};
static ucf::ScreenMark          g_marks[ucf::MAX_PLAYERS + 1]{};

// ImGui_ImplWin32_WndProcHandler 已在 <imgui_impl_win32.h> 中声明，直接调用即可。
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return TRUE;
    return DefWindowProcW(h, m, w, l);
}

static void cleanup_render_target() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static bool create_render_target(int w, int h) {
    cleanup_render_target();
    ID3D11Texture2D* back = nullptr;
    if (FAILED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    HRESULT hr = g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
    return SUCCEEDED(hr);
}

// 把 HRESULT 错误弹窗显示，方便在虚拟机里看到具体失败原因。
static void ucf_show_hr(const char* stage, HRESULT hr) {
    wchar_t buf[256];
    swprintf(buf, 256, L"%hs 失败: 0x%08X", stage, (unsigned long)hr);
    MessageBoxW(nullptr, buf, L"UCF Overlay", MB_OK | MB_ICONERROR);
}

static bool init_d3d11(HWND hwnd) {
    HRESULT hr;

    // 1) 枚举适配器：跳过 GameViewer（Status=Error 也可能排第一），
    //    优先选 VMware SVGA；都没有则回退默认硬件。
    IDXGIFactory2* factory = nullptr;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { ucf_show_hr("CreateDXGIFactory2", hr); return false; }

    IDXGIAdapter1* chosen = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (wcsstr(desc.Description, L"GameViewer") != nullptr) {
            adapter->Release(); adapter = nullptr; continue;
        }
        if (wcsstr(desc.Description, L"VMware") != nullptr) {
            if (chosen) chosen->Release();
            chosen = adapter; adapter = nullptr;
            break;                               // 命中 VMware，直接用
        }
        if (!chosen) { chosen = adapter; adapter = nullptr; }
        else         { adapter->Release(); adapter = nullptr; }
    }

    // 2) 创建设备：优先选中适配器，否则默认硬件，最后 WARP 软件降级。
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;

    if (chosen) {
        hr = D3D11CreateDevice(chosen, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                               levels, 3, D3D11_SDK_VERSION, &g_device, &got, &g_ctx);
    } else {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                               levels, 3, D3D11_SDK_VERSION, &g_device, &got, &g_ctx);
    }
    if (chosen) { chosen->Release(); chosen = nullptr; }

    if (FAILED(hr)) {
        ucf_show_hr("D3D11CreateDevice(HARDWARE)", hr);
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                               levels, 3, D3D11_SDK_VERSION, &g_device, &got, &g_ctx);
        if (FAILED(hr)) { ucf_show_hr("D3D11CreateDevice(WARP)", hr); factory->Release(); return false; }
    }

    // 3) 交换链：HWND 模式 + DISCARD（虚拟机对 FLIP 支持差），透明靠 DwmExtendFrameIntoClientArea。
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferDesc.Width  = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 1;
    sd.BufferDesc.RefreshRate.Denominator = 60;
    sd.SampleDesc.Count   = 1;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount        = 1;
    sd.OutputWindow       = hwnd;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = factory->CreateSwapChainForHwnd(g_device, hwnd, &sd, nullptr, nullptr, &g_swap);
    factory->Release();
    if (FAILED(hr)) { ucf_show_hr("CreateSwapChainForHwnd", hr); return false; }

    RECT rc; GetClientRect(hwnd, &rc);
    create_render_target(rc.right - rc.left, rc.bottom - rc.top);

    // 整窗客户区由含 alpha 的 D3D 表面透出，实现透明叠加。
    MARGINS m{-1};
    DwmExtendFrameIntoClientArea(hwnd, &m);
    return true;
}

static void frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ucf::Frame f{};
    ucf::SharedTransport shm("UcfFrame");
    if (shm.ok()) shm.read(f);
    else          g_synth.update(f);          // 无共享内存：自演

    int n = 0;
    ucf::project_frame(f, g_marks, n);

    RECT rc; GetClientRect(g_hwnd, &rc);
    float cw = float(rc.right - rc.left), ch = float(rc.bottom - rc.top);
    ucf::Viewport vp{0, 0, cw, ch, cw / float(f.width), ch / float(f.height)};

    ucf::DrawList dl{};
    if (g_settings.esp_enabled) ucf::build_draw_list(vp, g_marks, n, dl);

    ImDrawList* bdl = ImGui::GetBackgroundDrawList();
    ucf::render_draw_list(bdl, dl);
    ucf::draw_menu(g_settings, g_show_menu);

    ImGui::Render();
    const float clear[4] = {0, 0, 0, 0};      // 透明清空
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(1, 0);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.lpszClassName = L"UcfOverlay";
    RegisterClassExW(&wc);

    // 全屏、无边框、永远置顶、点击穿透
    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"UCF Overlay",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hinst, nullptr);

    if (!init_d3d11(g_hwnd)) {
        MessageBoxW(nullptr, L"D3D11 初始化失败（需要 Windows + d3d11/dxdgi）", L"UCF", MB_OK);
        return 1;
    }
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ucf::load_settings(g_settings, "ucf_overlay.ini");   // 失败则用默认值
    ShowWindow(g_hwnd, SW_SHOW);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_SIZE) {               // 处理 DPI/分辨率变化
                RECT r; GetClientRect(g_hwnd, &r);
                g_swap->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                create_render_target(r.right - r.left, r.bottom - r.top);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }
        frame();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_render_target();
    if (g_swap)  g_swap->Release();
    if (g_ctx)   g_ctx->Release();
    if (g_device) g_device->Release();
    return 0;
}
#else
// 非 Windows：占位，确保文件在任何平台都能被当作普通翻译单元编译（实际不会进构建）。
int main() { return 0; }
#endif
