// UCF 透明叠加层（Windows）：Win32 + D3D11 + ImGui。
// 透明机制（二选一，按交换链能力自动切换）：
//   - FLIP 模型（默认）：WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE，
//     交换链用 FLIP_DISCARD + PREMULTIPLIED，靠 swap chain 自身的 alpha 透出。
//     【注意】flip 交换链禁止与 WS_EX_LAYERED 同用，否则 CreateSwapChain 报 0x887A0001。
//   - BLT 回退（虚拟机不支持 flip 时）：补加 WS_EX_LAYERED + 黑色 colorkey，纯黑=透明。
//   - TRANSPARENT -> 点击穿透（动态开关：光标悬停菜单时移除，让菜单可交互）；
//     TOPMOST -> 永远盖在游戏之上。
// 数据来自 SharedTransport（Python 宿主写入）；没有共享内存时退回 SyntheticSource 自演。
//
// 仅在 WIN32 下编译/运行。Linux 上这个文件不参与构建（见 CMakeLists.txt）。
#include "overlay_win32.hpp"
#include "menu_win32.hpp"
#include "config.hpp"
#include "shared_state.hpp"
#include "smooth.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>           // CreateDXGIFactory2
#include <dwmapi.h>
#include <cwchar>               // swprintf
#include <cstdio>               // snprintf / fopen（诊断日志）
#include <cmath>
#include <algorithm>
#include <memory>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <string>
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
static std::unique_ptr<ucf::SharedTransport> g_shm;
static ucf::ScreenMark          g_marks[ucf::MAX_PLAYERS + 1]{};
static bool                     g_exit_requested = false;
static bool                     g_record_start_requested = false;
static bool                     g_record_stop_requested = false;
static FILE*                    g_record_file = nullptr;
static int                      g_recorded_frames = 0;
static int                      g_record_sample_counter = 0;
static std::chrono::steady_clock::time_point g_record_started;
static bool                     g_record_open_failed = false;
static bool                     g_flip_used = false;   // 交换链实际用的模型（诊断 HUD 用）
static bool                     g_clear_transparent = true;  // 清屏是否透明(alpha=0)；BLT colorkey 时为不透明黑

// ImGui_ImplWin32_WndProcHandler 已在 <imgui_impl_win32.h> 中声明，直接调用即可。
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_MOUSEACTIVATE) return MA_ACTIVATE;
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return TRUE;
    return DefWindowProcW(h, m, w, l);
}

// 每次写日志都立即关闭句柄，退出路径不留下持有中的日志文件。
static void append_debug_log(const char* message) {
    FILE* lf = std::fopen("ucf_debug.log", "a");
    if (!lf) return;
    std::fprintf(lf, "%s\n", message);
    std::fflush(lf);
    std::fclose(lf);
}

static void resolve_record_path() {
    char module_path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (!length || length >= MAX_PATH) return;
    std::filesystem::path exe_dir(module_path);
    exe_dir = exe_dir.parent_path();
    std::filesystem::path configured(g_settings.dev_record_path);
    if (configured.empty()) configured = "dev_frames.jsonl";
    if (configured.is_relative()) configured = exe_dir / configured;
    const std::string resolved = configured.lexically_normal().string();
    std::strncpy(g_settings.dev_record_path, resolved.c_str(),
                 sizeof(g_settings.dev_record_path) - 1);
    g_settings.dev_record_path[sizeof(g_settings.dev_record_path) - 1] = 0;
}

static void json_text(FILE* f, const char* text) {
    std::fputc('"', f);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text ? text : ""); *p; ++p) {
        if (*p == '"' || *p == '\\') std::fprintf(f, "\\%c", *p);
        else if (*p == '\n') std::fputs("\\n", f);
        else if (*p == '\r') std::fputs("\\r", f);
        else if (*p == '\t') std::fputs("\\t", f);
        else if (*p >= 0x20) std::fputc(*p, f);
    }
    std::fputc('"', f);
}

static void write_bones_json(FILE* f, const ucf::PlayerState& p, bool include_bones) {
    if (!include_bones) { std::fputs("[]", f); return; }
    std::fputc('[', f);
    for (int i = 0; i < ucf::MAX_BONES; ++i) {
        if (i) std::fputc(',', f);
        const auto& b = p.bones[i];
        std::fprintf(f, "{\"pos\":[%.6g,%.6g,%.6g],\"valid\":%s}",
                     b.pos[0], b.pos[1], b.pos[2], b.valid ? "true" : "false");
    }
    std::fputc(']', f);
}

static void write_player_json(FILE* f, const ucf::PlayerState& p, bool include_bones, bool include_name) {
    std::fprintf(f, "{\"pos\":[%.6g,%.6g,%.6g],\"team\":%d,\"hp\":%d,\"maxHp\":%d,\"isDead\":%s",
                 p.pos[0], p.pos[1], p.pos[2], p.team, p.hp, p.maxHp, p.isDead ? "true" : "false");
    if (include_name) { std::fputs(",\"name\":", f); json_text(f, p.name); }
    std::fputs(",\"bones\":", f); write_bones_json(f, p, include_bones);
    std::fputc('}', f);
}

static void close_recording() {
    if (!g_record_file) return;
    std::fflush(g_record_file);
    std::fclose(g_record_file);
    g_record_file = nullptr;
    append_debug_log("dev: recording stopped");
}

static void open_recording() {
    if (g_record_open_failed) return;
    close_recording();
    resolve_record_path();
    std::error_code ec;
    const std::filesystem::path path(g_settings.dev_record_path);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    g_record_file = std::fopen(g_settings.dev_record_path, "a");
    g_recorded_frames = 0;
    g_record_sample_counter = 0;
    g_record_started = std::chrono::steady_clock::now();
    if (g_record_file) {
        append_debug_log("dev: recording started");
    } else {
        char message[512];
        std::snprintf(message, sizeof(message),
                      "dev: recording open failed: errno=%d path=%s", errno,
                      g_settings.dev_record_path);
        append_debug_log(message);
        g_record_open_failed = true;
        g_settings.dev_record_enabled = false;
    }
}

static void record_frame(const ucf::Frame& f) {
    if (g_record_stop_requested) {
        close_recording();
        g_record_stop_requested = false;
    }
    if (g_record_start_requested) {
        g_record_open_failed = false;
        open_recording();
        g_record_start_requested = false;
    }
    if (!g_settings.dev_record_enabled) { close_recording(); return; }
    if (!g_record_file && !g_record_open_failed) open_recording();
    if (!g_record_file) return;
    ++g_record_sample_counter;
    const int every = g_settings.dev_record_every < 1 ? 1 : g_settings.dev_record_every;
    if (g_record_sample_counter % every != 0) return;
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_record_started).count();
    if (g_settings.dev_record_duration > 0.0f && elapsed >= g_settings.dev_record_duration) {
        g_settings.dev_record_enabled = false;
        close_recording();
        return;
    }
    if (g_settings.dev_record_max_frames > 0 && g_recorded_frames >= g_settings.dev_record_max_frames) {
        g_settings.dev_record_enabled = false;
        close_recording();
        return;
    }
    std::fprintf(g_record_file, "{\"schema_version\":2,\"seq\":%d,\"timestamp\":%.6f,\"type\":\"frame\",\"w2c\":[",
                 g_recorded_frames, elapsed);
    for (int i = 0; i < 16; ++i) std::fprintf(g_record_file, "%s%.9g", i ? "," : "", f.w2c[i]);
    std::fputs("],\"proj\":[", g_record_file);
    for (int i = 0; i < 16; ++i) std::fprintf(g_record_file, "%s%.9g", i ? "," : "", f.proj[i]);
    std::fprintf(g_record_file, "],\"width\":%d,\"height\":%d,\"inGame\":%s,\"local\":",
                 f.width, f.height, f.inGame ? "true" : "false");
    write_player_json(g_record_file, f.local, g_settings.dev_record_bones, g_settings.dev_record_name);
    std::fputs(",\"players\":[", g_record_file);
    const int count = (f.playerCount < 0) ? 0 : (f.playerCount > ucf::MAX_PLAYERS ? ucf::MAX_PLAYERS : f.playerCount);
    for (int i = 0; i < count; ++i) {
        if (i) std::fputc(',', g_record_file);
        write_player_json(g_record_file, f.players[i], g_settings.dev_record_bones, g_settings.dev_record_name);
    }
    std::fprintf(g_record_file, "],\"playerCount\":%d}\n", count);
    std::fflush(g_record_file);
    ++g_recorded_frames;
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

// 热键轮询（edge 触发，键位可在 ucf_overlay.ini 配置）：
//   HOME   -> 切换菜单显隐；DELETE -> 切换 ESP 绘制层显隐。
// GetAsyncKeyState 不需要窗口焦点，无焦点的叠加层也能收到。
static void poll_hotkeys() {
    auto pressed = [](int vk) -> bool {
        static bool down[256] = {};
        if (vk < 0 || vk > 255) return false;
        bool now  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool edge = now && !down[vk];
        down[vk] = now;
        return edge;
    };
    if (pressed(g_settings.menu_hotkey)) g_show_menu = !g_show_menu;
    if (pressed(g_settings.esp_hotkey))  g_settings.esp_visible = !g_settings.esp_visible;
    if (pressed(VK_END))                 g_exit_requested = true;
}

// 点击穿透动态开关：
//   常态带 WS_EX_TRANSPARENT（整窗不吃鼠标，点击落到游戏/桌面）；
//   光标进入菜单窗口矩形（或 ImGui 正在拖拽控件）时临时移除，让菜单可交互。
// 为什么不用 WM_NCHITTEST 返回 HTTRANSPARENT：它只保证同线程窗口间的穿透，
// 跨进程（文件管理器/游戏）不可靠；动态 WS_EX_TRANSPARENT 是标准做法。
// 注意：带 WS_EX_TRANSPARENT 时收不到任何鼠标消息，因此只能每帧轮询 GetCursorPos。
static void update_click_through() {
    bool interactive = false;
    if (g_show_menu) {
        const ucf::MenuRect r = ucf::query_menu_rect();
        POINT pt{};
        RECT wr{};
        if (r.valid && GetCursorPos(&pt) && GetWindowRect(g_hwnd, &wr)) {
            const float lx = float(pt.x - wr.left), ly = float(pt.y - wr.top);
            const float pad = 8.0f;
            interactive = (lx >= r.x - pad && lx <= r.x + r.w + pad &&
                           ly >= r.y - pad && ly <= r.y + r.h + pad);
        }
    }
    if (ImGui::IsAnyMouseDown()) interactive = true;   // 拖拽滑块过程中保持可交互
    const LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    const LONG_PTR interactive_bits = (LONG_PTR)(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    const LONG_PTR want = interactive ? (ex & ~interactive_bits)
                                      : (ex | interactive_bits);
    if (want != ex) {
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, want);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (interactive) {
            SetForegroundWindow(g_hwnd);
            SetFocus(g_hwnd);
        }
    }
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
    poll_hotkeys();
    update_click_through();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ucf::Frame f{};
    const auto read_begin = std::chrono::steady_clock::now();
    if (!g_shm) g_shm = std::make_unique<ucf::SharedTransport>("UcfFrame");
    bool have_data = g_shm->ok();
    if (have_data) {
        g_shm->read(f);
        // 共享内存可能已存在但尚无生产者写入（全零 Frame）-> 尺寸非法，回退自演
        have_data = (f.width > 0 && f.height > 0);
    }
    if (!have_data) g_synth.update(f);          // 无有效共享内存：自演
    const float read_ms = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - read_begin).count();
    record_frame(f);

    int n = 0;
    const auto project_begin = std::chrono::steady_clock::now();
    ucf::project_frame(f, g_marks, n);
    const float project_ms = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - project_begin).count();

    RECT rc; GetClientRect(g_hwnd, &rc);
    float cw = float(rc.right - rc.left), ch = float(rc.bottom - rc.top);
    float fw = (f.width  > 0) ? float(f.width)  : 1280.0f;   // 分母兜底，避免 scale=inf
    float fh = (f.height > 0) ? float(f.height) : 720.0f;
    ucf::Viewport vp{0, 0, cw, ch, cw / fw, ch / fh};

    ucf::DrawList dl{};

    // 目标选择与角度平滑只产生诊断结果，不写鼠标、不写输入。
    static ucf::Angles output_angles{};
    int target = -1;
    if (g_settings.aimbot_enabled && f.inGame) {
        ucf::Candidate candidates[ucf::MAX_PLAYERS]{};
        const int player_count = std::max(0, std::min(f.playerCount, ucf::MAX_PLAYERS));
        for (int i = 0; i < player_count; ++i) {
            const auto& p = f.players[i];
            const float dx = p.pos[0] - f.local.pos[0];
            const float dy = p.pos[1] - f.local.pos[1];
            const float dz = p.pos[2] - f.local.pos[2];
            candidates[i].dir = {dx, dy, dz};
            candidates[i].dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            candidates[i].hp = p.hp;
            candidates[i].team = p.team;
            candidates[i].dead = p.isDead || p.hp <= 0;
            candidates[i].visible = g_marks[i + 1].on_screen;
            if ((!g_settings.aim_teammates && p.team == f.local.team) ||
                (!g_settings.aim_dead && candidates[i].dead) ||
                (g_settings.aim_visible_only && !candidates[i].visible) ||
                (g_settings.aim_max_distance > 0.0f && candidates[i].dist > g_settings.aim_max_distance)) {
                candidates[i].valid = false;
            }
        }
        target = ucf::select_target({0, 0, 1}, candidates, player_count,
                                    g_settings.fov_deg, g_settings.target_mode);
        if (target >= 0) {
            output_angles = ucf::smooth_angles(output_angles,
                ucf::angles_from_direction(candidates[target].dir),
                g_settings.responsiveness);
        }
    }

    if (g_settings.esp_enabled && g_settings.esp_visible) {
        ucf::DrawStyle style{};
        style.show_box = g_settings.show_box;
        style.show_skeleton = g_settings.show_skeleton;
        style.show_health = g_settings.show_health;
        style.show_distance = g_settings.show_distance;
        style.show_local = g_settings.show_local;
        style.show_teammate = g_settings.show_teammate;
        style.show_enemy = g_settings.show_enemy;
        style.target_index = target >= 0 ? target + 1 : -1;
        style.show_target_ray = g_settings.show_target_ray;
        style.ray_from_bottom = g_settings.ray_from_bottom;
        style.max_distance = g_settings.max_distance;
        style.line_thickness = g_settings.line_thickness;
        std::copy(g_settings.color_local, g_settings.color_local + 3, style.local);
        std::copy(g_settings.color_teammate, g_settings.color_teammate + 3, style.teammate);
        std::copy(g_settings.color_enemy, g_settings.color_enemy + 3, style.enemy);
        ucf::build_draw_list(vp, g_marks, n, style, dl);
    }

    ImDrawList* bdl = ImGui::GetBackgroundDrawList();
    if (g_settings.show_fov_circle) {
        const float fov = std::max(1.0f, std::min(180.0f, g_settings.fov_deg));
        const float radius = (fov / 90.0f) * (std::min(vp.w, vp.h) * 0.5f);
        bdl->AddCircle(ImVec2(vp.x + vp.w * 0.5f, vp.y + vp.h * 0.5f),
                       radius, IM_COL32(255, 220, 80, 180), 96, 1.0f);
    }
    const auto draw_begin = std::chrono::steady_clock::now();
    ucf::render_draw_list(bdl, dl);
    const float draw_ms = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - draw_begin).count();

    if (g_settings.debug_show_annotations || g_settings.debug_show_bones) {
        static const char* bone_names[] = {
            "Hips", "LeftUpperLeg", "RightUpperLeg", "LeftLowerLeg", "RightLowerLeg",
            "LeftFoot", "RightFoot", "Spine", "Chest", "Neck", "Head",
            "LeftShoulder", "RightShoulder", "LeftUpperArm", "RightUpperArm",
            "LeftLowerArm", "RightLowerArm", "LeftHand", "RightHand"
        };
        for (int i = 0; i < n + 1; ++i) {
            const auto& mark = g_marks[i];
            const float x = vp.x + mark.sx * vp.sx;
            const float y = vp.y + mark.sy * vp.sy;
            char text[96]{};
            if (g_settings.debug_show_annotations && mark.on_screen) {
                std::snprintf(text, sizeof(text), "P%d d=%.1fm hp=%d", i, mark.dist, mark.hp);
                bdl->AddText(ImVec2(x + 4.0f, y - 14.0f), IM_COL32(255, 255, 0, 230), text);
            }
            if (g_settings.debug_show_bones) {
                for (int b = 0; b < ucf::MAX_BONES; ++b) {
                    if (!mark.bones[b].valid) continue;
                    const float bx = vp.x + mark.bones[b].sx * vp.sx;
                    const float by = vp.y + mark.bones[b].sy * vp.sy;
                    std::snprintf(text, sizeof(text), "%d:%s", b, bone_names[b]);
                    bdl->AddText(ImVec2(bx + 2.0f, by), IM_COL32(180, 220, 255, 220), text);
                }
            }
        }
    }

    // 诊断信息收进菜单（透明窗口验证通过后，移除全屏 HUD 与强制红框，避免遮挡画面）。
    static unsigned long last_present_hr = 0;
    ucf::OverlayStatus st{};
    st.flip = g_flip_used;
    st.shm = have_data;
    st.players = n;
    st.scale = cw / fw;
    st.present_hr = last_present_hr;
    st.target = target;
    st.target_yaw = output_angles.yaw;
    st.target_pitch = output_angles.pitch;
    st.target_delta = (target >= 0 && target < f.playerCount && target < ucf::MAX_PLAYERS) ? ucf::angle_distance(output_angles,
        ucf::angles_from_direction({f.players[target].pos[0] - f.local.pos[0],
                                    f.players[target].pos[1] - f.local.pos[1],
                                    f.players[target].pos[2] - f.local.pos[2]})) : 0.0f;
    st.bones_total = n * ucf::MAX_BONES;
    st.bones_valid = 0;
    for (int i = 0; i < n; ++i)
        for (int b = 0; b < ucf::MAX_BONES; ++b)
            if (g_marks[i].bones[b].valid) ++st.bones_valid;
    static int frame_index = 0;
    st.frame_index = frame_index++;
    st.read_ms = read_ms;
    st.project_ms = project_ms;
    st.draw_ms = draw_ms;
    st.recorded_frames = g_recorded_frames;
    st.recording = g_record_file != nullptr;
    ucf::draw_menu(g_settings, g_show_menu, g_exit_requested,
                   g_record_start_requested, g_record_stop_requested, st);

    ImGui::Render();
    // flip 透明路径：清空 (0,0,0,0)；BLT colorkey 路径：清空不透明黑 (0,0,0,255)。
    const float clear[4] = {0.0f, 0.0f, 0.0f, g_clear_transparent ? 0.0f : 1.0f};
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    HRESULT present_hr = g_swap->Present(1, 0);
    last_present_hr = (unsigned long)present_hr;

    // 心跳日志（相对 exe 目录）：每 120 帧写一行，便于远程确认程序在跑。
    {
        static long dbg_frames = 0;
        if ((dbg_frames++ % 120) == 0) {
            FILE* lf = std::fopen("ucf_debug.log", "a");
            if (lf) {
                fprintf(lf, "[%ld] mode=%s menu=%d esp=%d win=%dx%d frame=%dx%d players=%d scale=%.2f src=%s clear=%s present_hr=0x%08X\n",
                        dbg_frames, g_flip_used ? "FLIP" : "BLT",
                        g_show_menu ? 1 : 0, g_settings.esp_visible ? 1 : 0,
                        int(cw), int(ch), f.width, f.height,
                        n, cw / fw, have_data ? "shm" : "synth",
                        g_clear_transparent ? "transp" : "black",
                        (unsigned long)present_hr);
                fclose(lf);
            }
        }
    }
}

// 加载系统中文（CJK）字体，让 ImGui 菜单能显示中文。
// 失败（如 VM 缺字体文件）则保留默认 ASCII 字体，标签仍可用英文兜底。
// 注意：Build() 在首帧 NewFrame 时自动触发，这里只需在 CreateContext 后 AddFont 即可。
static void init_overlay_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    static const char* cands[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
    };
    ImFont* cjk = nullptr;
    for (const char* p : cands) {
        cjk = io.Fonts->AddFontFromFileTTF(p, 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
        if (cjk) break;
    }
    if (cjk) io.FontDefault = cjk;
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.lpszClassName = L"UcfOverlay";
    RegisterClassExW(&wc);

    // 全屏、无边框、永远置顶。WS_EX_TRANSPARENT 是动态开关（见 update_click_through）：
    // 光标悬停菜单时移除以让菜单可交互，其余时间保持点击穿透。
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
    init_overlay_fonts();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ucf::load_settings(g_settings, "ucf_overlay.ini");   // 失败则用默认值
    if (g_settings.menu_hotkey == 0x2D /*VK_INSERT*/)    // 旧 ini 存的是 INSERT：迁移为 HOME
        g_settings.menu_hotkey = VK_HOME;
    resolve_record_path();
    ShowWindow(g_hwnd, SW_SHOW);
    append_debug_log("startup: overlay initialized");

    MSG msg{};
    while (msg.message != WM_QUIT && !g_exit_requested) {
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

    append_debug_log("exit: saving settings");
    close_recording();
    append_debug_log("exit: recording closed");
    ucf::save_settings(g_settings, "ucf_overlay.ini");
    append_debug_log("exit: settings saved");

    // SharedTransport 析构总是执行 UnmapViewOfFile + CloseHandle；Windows
    // 映射对象不做删除操作，最后一个句柄释放后由系统回收。
    g_shm.reset();
    append_debug_log("exit: shared memory mapping closed");

    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
    append_debug_log("exit: window destroyed");

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    append_debug_log("exit: imgui shutdown");
    cleanup_render_target();
    if (g_swap)  g_swap->Release();
    if (g_ctx)   g_ctx->Release();
    if (g_device) g_device->Release();
    append_debug_log("exit: d3d destroyed");
    if (g_settings.exit_delete_config) {
        std::remove("ucf_overlay.ini");
        append_debug_log("exit: deleted ucf_overlay.ini");
    }
    if (g_settings.exit_delete_log) {
        append_debug_log("exit: deleting ucf_debug.log");
        std::remove("ucf_debug.log");
    }
    return 0;
}
#else
// 非 Windows：占位，确保文件在任何平台都能被当作普通翻译单元编译（实际不会进构建）。
int main() { return 0; }
#endif
