// UCF 透明叠加层（Windows）：Win32 + D3D11 + ImGui。
// 透明机制（二选一，按交换链能力自动切换）：
//   - FLIP 模型（默认）：WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE，
//     交换链用 FLIP_DISCARD + PREMULTIPLIED，靠 swap chain 自身的 alpha 透出。
//     【注意】flip 交换链禁止与 WS_EX_LAYERED 同用，否则 CreateSwapChain 报 0x887A0001。
//   - BLT 回退（虚拟机不支持 flip 时）：补加 WS_EX_LAYERED + 黑色 colorkey，纯黑=透明。
//   - TRANSPARENT -> 整窗不吃鼠标（点击穿透到下面的游戏）；TOPMOST -> 永远盖在游戏之上。
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
#include <cwchar>               // swprintf
#include <cstdio>               // snprintf / fopen（诊断日志）
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
static bool                     g_flip_used = false;   // 交换链实际用的模型（诊断 HUD 用）
static bool                     g_clear_transparent = true;  // 清屏是否透明(alpha=0)；BLT colorkey 时为不透明黑

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

    // 3) 交换链透明度方案
    //    - 方案 A（默认）：FLIP_DISCARD + PREMULTIPLIED，原生逐像素透明。
    //      关键：flip 模型交换链【禁止】与 WS_EX_LAYERED 同用（否则 0x887A0001），
    //      因此窗口创建时未带 WS_EX_LAYERED，透明仅靠 swap chain 的 alpha。
    //    - 方案 B（回退）：blt 模型 + WS_EX_LAYERED + 黑色 colorkey，纯黑像素透明
    //      （兼容不支持 flip 的虚拟机）。
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Format     = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count   = 1;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.Flags              = 0;   // flip 模型不允许 ALLOW_MODE_SWITCH，否则 INVALID_CALL

    // 方案 A：flip 模型 + 预乘 alpha，逐像素透明。
    sd.BufferCount = 2;                       // flip 模型要求 >= 2
    sd.Scaling     = DXGI_SCALING_NONE;
    sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;
    HRESULT flip_hr = factory->CreateSwapChainForHwnd(g_device, hwnd, &sd, nullptr, nullptr, &g_swap);
    g_flip_used = SUCCEEDED(flip_hr);

    HRESULT blt_hr = flip_hr;
    if (FAILED(flip_hr)) {
        // 方案 B 回退：blt 模型（UNSPECIFIED，缓冲不透明）+ 分层窗口黑色 colorkey 透明。
        sd.BufferCount = 1;
        sd.Scaling     = DXGI_SCALING_STRETCH;
        sd.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
        sd.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
        blt_hr = factory->CreateSwapChainForHwnd(g_device, hwnd, &sd, nullptr, nullptr, &g_swap);
    }
    factory->Release();

    if (FAILED(blt_hr)) { ucf_show_hr("CreateSwapChainForHwnd", blt_hr); return false; }

    RECT rc; GetClientRect(hwnd, &rc);
    create_render_target(rc.right - rc.left, rc.bottom - rc.top);

    // 透明机制：
    //   flip -> 靠 swap chain 预乘 alpha（配合 DwmExtend 把桌面透出）；清屏 (0,0,0,0)。
    //   blt  -> 给窗口补 WS_EX_LAYERED + 黑色 colorkey（纯黑=透明）；清屏不透明黑 (0,0,0,255)。
    //   注：未设分层属性的 WS_EX_LAYERED 窗口默认整体不可见，故 blt 路径必须显式 SetLayeredWindowAttributes。
    HRESULT dwm_hr = S_OK;
    if (g_flip_used) {
        MARGINS m{-1};
        dwm_hr = DwmExtendFrameIntoClientArea(hwnd, &m);
        g_clear_transparent = true;
    } else {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                          GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        g_clear_transparent = false;
    }

    // 启动日志（覆盖写）：记录交换链选择、DWM/分层结果，便于远程诊断。
    {
        FILE* lf = std::fopen("ucf_debug.log", "w");
        if (lf) {
            fprintf(lf, "init: flip_hr=0x%08X used=%d blt_hr=0x%08X dwm_hr=0x%08X clear=%s\n",
                    (unsigned long)flip_hr, g_flip_used ? 1 : 0, (unsigned long)blt_hr,
                    (unsigned long)dwm_hr, g_clear_transparent ? "transparent" : "black-colorkey");
            fclose(lf);
        }
    }
    return true;
}

static void frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ucf::Frame f{};
    ucf::SharedTransport shm("UcfFrame");
    bool have_data = shm.ok();
    if (have_data) {
        shm.read(f);
        // 共享内存可能已存在但尚无生产者写入（全零 Frame）-> 尺寸非法，回退自演
        have_data = (f.width > 0 && f.height > 0);
    }
    if (!have_data) g_synth.update(f);          // 无有效共享内存：自演

    int n = 0;
    ucf::project_frame(f, g_marks, n);

    RECT rc; GetClientRect(g_hwnd, &rc);
    float cw = float(rc.right - rc.left), ch = float(rc.bottom - rc.top);
    float fw = (f.width  > 0) ? float(f.width)  : 1280.0f;   // 分母兜底，避免 scale=inf
    float fh = (f.height > 0) ? float(f.height) : 720.0f;
    ucf::Viewport vp{0, 0, cw, ch, cw / fw, ch / fh};

    ucf::DrawList dl{};
    if (g_settings.esp_enabled) ucf::build_draw_list(vp, g_marks, n, dl);

    ImDrawList* bdl = ImGui::GetBackgroundDrawList();
    ucf::render_draw_list(bdl, dl);

    // 诊断 HUD：始终绘制，确认渲染管线上线（不依赖菜单显隐）。
    {
        char hud[256];
        std::snprintf(hud, sizeof(hud),
                     "UCF Overlay  mode:%s  menu:%s\n"
                     "win:%dx%d  frame:%dx%d\n"
                     "players:%d  scale:%.2f  src:%s  clear:%s",
                     g_flip_used ? "FLIP" : "BLT",
                     g_show_menu ? "ON" : "OFF",
                     int(cw), int(ch), f.width, f.height,
                     n, cw / fw,
                     have_data ? "shm" : "synth",
                     g_clear_transparent ? "transp" : "black");
        ImVec2 p(12, 12);
        bdl->AddRectFilled(p, ImVec2(p.x + 360, p.y + 72), IM_COL32(0, 0, 0, 230));   // 不透明黑底
        bdl->AddText(ImVec2(p.x + 6, p.y + 6), IM_COL32(255, 255, 255, 255), hud);   // 白字高对比
    }

    // 强制红框：验证渲染管线是否真的画到屏幕（诊断用，稳定后可删）。
    bdl->AddRectFilled(ImVec2(100, 100), ImVec2(600, 500), IM_COL32(255, 0, 0, 255));

    ucf::draw_menu(g_settings, g_show_menu);

    ImGui::Render();
    // flip 透明路径：清空 (0,0,0,0)；BLT colorkey 路径：清空不透明黑 (0,0,0,255)。
    const float clear[4] = {0.0f, 0.0f, 0.0f, g_clear_transparent ? 0.0f : 1.0f};
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    HRESULT present_hr = g_swap->Present(1, 0);

    // 心跳日志（相对 exe 目录）：每 120 帧写一行，便于远程确认程序在跑。
    {
        static long dbg_frames = 0;
        if ((dbg_frames++ % 120) == 0) {
            FILE* lf = std::fopen("ucf_debug.log", "a");
            if (lf) {
                fprintf(lf, "[%ld] mode=%s menu=%d win=%dx%d frame=%dx%d players=%d scale=%.2f src=%s clear=%s present_hr=0x%08X\n",
                        dbg_frames, g_flip_used ? "FLIP" : "BLT",
                        g_show_menu ? 1 : 0, int(cw), int(ch), f.width, f.height,
                        n, cw / fw, have_data ? "shm" : "synth",
                        g_clear_transparent ? "transp" : "black",
                        (unsigned long)present_hr);
                fclose(lf);
            }
        }
    }
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.lpszClassName = L"UcfOverlay";
    RegisterClassExW(&wc);

    // 全屏、无边框、永远置顶、点击穿透。
    // 注意：这里【不能】带 WS_EX_LAYERED —— flip 模型交换链与之互斥（会 0x887A0001）。
    // 透明由 flip 交换链的 alpha 或 BLT 回退时的 colorkey 提供。
    g_hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
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
