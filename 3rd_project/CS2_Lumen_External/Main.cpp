#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <mciapi.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <array>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "Features/AvatarManager.h"
#include "memory.h"
#include "driver.h"
#include "Offsets.h"
#include "SDK/Game.h"
#include "SDK/Structs.h"
#include "Features/ESP.h"
#include "Features/Aimbot.h"
#include "Features/Bomb.h"

Offsets g_Off;
Memory* g_Memory = nullptr;
uintptr_t g_Client = 0;
uintptr_t g_Engine = 0;
std::atomic<bool> g_Running = true;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Winmm.lib")

// ──────────────────────────────────────────────
//  SKIN CHANGER OFFSETS  (verified from dumper)
// ──────────────────────────────────────────────
// Offsets are now handled by g_Off in Offsets.h

// ──────────────────────────────────────────────
//  SETTINGS
// ──────────────────────────────────────────────
namespace Settings {
    bool menu_visible    = true;
    bool skin_enabled    = false;
    float wear           = 0.001f;
    int stattrak_val     = 0;

    // ── Player Visualization ──
    bool esp_enabled     = false;
    bool avatar_esp      = true;
    bool health_bar      = true;
    bool armor_bar       = false;
    bool skeleton        = false;
    bool names           = true;
    bool distance        = false;
    bool snaplines       = false;
    bool head_dot        = false;
    bool box_3d          = false;
    bool grenade_prediction = false;
    bool fake_glow_enemies = false;
    bool fake_chams = false;
    bool rainbow_global  = false;
    bool esp_enemies_only = true;
    float rgb_speed      = 1.0f;
    float glow_spread    = 1.0f;
    bool  hitmarker      = true;
    float hitmarker_size = 1.0f;

    // ── Box Styles & Effects ──
    bool box_2d          = true;
    int  box_style       = 0;
    bool fill_box        = false;
    bool outline_box     = false;
    bool rainbow_box     = false;
    float box_color[4]   = { 0.f, 0.f, 0.f, 1.f };

    // Index mapping: 0=Rifles, 1=SMGs, 2=Pistols, 3=Snipers
    WeaponConfig weapon_configs[4];
    int current_weapon_tab = 0;

    // ── Aimbot (Legit/Khytt) ──
    bool  aim_enabled      = false;
    bool  target_legit     = true;   // Uses mouse_event
    bool  safety_lock      = false;  // Memory write protection
    bool  aim_vis_check    = true;
    bool  aim_autowall     = false;
    bool  aimbot_humanized = true;
    float aim_jitter       = 0.5f;   // Khytt: aimbot_jitter_scale
    float sensitivity      = 1.0f;
    int   aim_key          = VK_RBUTTON;
    int   aim_key_idx      = 1;

    bool  target_lock_prevent = false;
    bool  lock_prevent_active  = false; // Internal state

    // ── Recoil Control (RCS) ──
    bool  rcs_standalone  = false;
    float rcs_scale       = 2.0f;

    // ── Extras ──
    bool  show_fov_circle       = true;
    float fov_circle_color[4]   = { 1.f, 1.f, 1.f, 0.6f };

    // ── System & Hitsounds ──
    bool watermark          = true;
    bool spectator_list     = false;
    bool bomb_esp           = false;
    bool bomb_timer_hud     = false;
    bool kernel_bypass      = false;
    bool hitsound           = false;
    int  hitsound_sel       = 0;
    std::vector<std::string> hitsound_files = {"Default", "Bello", "Skeet"};
    std::string exeDir;

    ImFont* font_main = nullptr;
    ImFont* font_icons = nullptr;
}

// ──────────────────────────────────────────────
//  SKIN TABLE
// ──────────────────────────────────────────────
struct SkinEntry {
    const char* weaponName;
    short weaponId;
    const char* skinName;
    int paintKit;
};

static SkinEntry g_SkinTable[] = {
    { "AK-47",      7,  "Cartel",         433 },
    { "AWP",        9,  "Dragon Lore",    344 },
    { "M4A4",      16,  "Hellfire",       609 },
    { "Glock-18",   4,  "Bunsen Burner",  437 },
    { "USP-S",     61,  "Kill Confirmed", 504 },
    { "Desert Eag", 1,  "Mecha Indust.",  711 },
};
static int g_SkinSelection[IM_ARRAYSIZE(g_SkinTable)];
static bool g_SkinOverride[IM_ARRAYSIZE(g_SkinTable)];

int GetWeaponPaint(short id) {
    for (int i = 0; i < IM_ARRAYSIZE(g_SkinTable); i++) {
        if (g_SkinTable[i].weaponId == id && g_SkinOverride[i])
            return g_SkinTable[i].paintKit;
    }
    return 0;
}

// ──────────────────────────────────────────────
//  SKIN CHANGER THREAD
// ──────────────────────────────────────────────
void SkinChangerThread() {
    while (g_Running) {
        if (!Settings::skin_enabled || !g_Client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        uintptr_t localPlayer = Game::GetLocalPlayerPawn();
        if (!localPlayer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        uintptr_t weapSvc = g_Memory->Read<uintptr_t>(localPlayer + g_Off.m_pWeaponServices);
        if (!weapSvc) continue;

        bool forceUpdate = false;
        uintptr_t entityList = Game::GetEntityList();

        for (int i = 0; i < 8; i++) {
            uint32_t handle = g_Memory->Read<uint32_t>(weapSvc + g_Off.m_hMyWeapons + (i * 0x4));
            if (handle == 0xFFFFFFFF || handle == 0) continue;

            uintptr_t weapon = Game::GetEntityFromHandle(handle, entityList);
            if (!weapon) continue;

            short itemID = g_Memory->Read<short>(weapon + g_Off.m_iItemDefinitionIndex);
            if (int paint = GetWeaponPaint(itemID)) {
                if (g_Memory->Read<int32_t>(weapon + g_Off.m_nFallbackPaintKit) != paint) {
                    g_Memory->Write<int32_t>(weapon + g_Off.m_iItemIDHigh, -1);
                    g_Memory->Write<int32_t>(weapon + g_Off.m_nFallbackPaintKit, paint);
                    g_Memory->Write<float>(weapon + g_Off.m_flFallbackWear, Settings::wear);
                    if (Settings::stattrak_val > 0)
                        g_Memory->Write<int32_t>(weapon + g_Off.m_nFallbackStatTrak, Settings::stattrak_val);
                    forceUpdate = true;
                }
            }
        }

        if (forceUpdate && g_Engine) {
            uintptr_t nc = g_Memory->Read<uintptr_t>(g_Engine + g_Off.dwNetworkGameClient);
            if (nc) g_Memory->Write<int32_t>(nc + g_Off.m_nDeltaTick, -1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ──────────────────────────────────────────────
//  OVERLAY
// ──────────────────────────────────────────────
static HWND               g_hWnd      = nullptr;
static ID3D11Device*      g_Device    = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static IDXGISwapChain*    g_SwapChain = nullptr;
static ID3D11RenderTargetView* g_RTV  = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool CreateOverlayWindow() {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, GetModuleHandle(NULL) };
    wc.lpszClassName = L"ExtOverlay";
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName, L"CS2 External",
        WS_POPUP, 0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr);

    SetLayeredWindowAttributes(g_hWnd, RGB(0,0,0), 0, LWA_COLORKEY);
    MARGINS m = {-1}; DwmExtendFrameIntoClientArea(g_hWnd, &m);
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);
    return true;
}

bool InitD3D() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed     = TRUE;
    sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fla[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fla, 2, D3D11_SDK_VERSION, &sd, &g_SwapChain, &g_Device, &fl, &g_Context) != S_OK)
        return false;

    ID3D11Texture2D* bb = nullptr;
    g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_Device->CreateRenderTargetView(bb, nullptr, &g_RTV);
    bb->Release();
    return true;
}

// ──────────────────────────────────────────────
//  MENU
// ──────────────────────────────────────────────
void ApplyMenuStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 0.f;
    s.ChildRounding     = 0.f;
    s.FrameRounding     = 2.f;
    s.GrabRounding      = 2.f;
    s.PopupRounding     = 2.f;
    s.ScrollbarRounding = 0.f;
    s.TabRounding       = 0.f;
    s.WindowPadding     = ImVec2(0, 0);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(8, 8);
    s.WindowBorderSize  = 1.f;
    s.ChildBorderSize   = 1.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_Border]            = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.40f, 0.45f, 0.95f, 1.00f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.40f, 0.45f, 0.95f, 1.00f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.50f, 0.55f, 1.00f, 1.00f);
    c[ImGuiCol_Button]            = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    c[ImGuiCol_Header]            = ImVec4(0.15f, 0.15f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.20f, 0.20f, 0.30f, 1.00f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.25f, 0.25f, 0.38f, 1.00f);
    c[ImGuiCol_Text]              = ImVec4(0.70f, 0.70f, 0.75f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
}

namespace UI {
    bool Checkbox(const char* label, bool* v) {
        ImGuiContext* g = ImGui::GetCurrentContext();
        if (!g) return false;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems) return false;

        const ImGuiStyle& style = g->Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        float height = 16.0f;
        float width = 32.0f;
        float radius = height * 0.5f;

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y > height ? label_size.y : height));

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id)) return false;

        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed) { 
            *v = !(*v); 
            ImGui::MarkItemEdited(id); 
        }

        float t = *v ? 1.0f : 0.0f;
        ImU32 col_bg = ImGui::GetColorU32(*v ? ImVec4(0.40f, 0.45f, 0.95f, 1.00f) : ImVec4(0.15f, 0.15f, 0.20f, 1.0f));
        
        window->DrawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), col_bg, radius);
        window->DrawList->AddCircleFilled(ImVec2(pos.x + radius + t * (width - radius * 2.0f), pos.y + radius), radius - 2.0f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

        if (label_size.x > 0.0f)
            ImGui::RenderText(ImVec2(pos.x + width + style.ItemInnerSpacing.x, pos.y + (height - label_size.y) * 0.5f), label); 
        return pressed;
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max) {
        ImGuiContext* g = ImGui::GetCurrentContext();
        if (!g) return false;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems) return false;

        const ImGuiStyle& style = g->Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const ImVec2 pos = window->DC.CursorPos;
        float w = ImGui::CalcItemWidth();
        float h = 18.0f;
        const ImRect frame_bb(pos, pos + ImVec2(w, h));
        const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) return false;

        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
        
        float v_new = *v;
        ImRect grab_bb;
        const bool value_changed = ImGui::SliderBehavior(frame_bb, id, ImGuiDataType_Float, &v_new, &v_min, &v_max, "%.2f", ImGuiSliderFlags_None, &grab_bb);
        if (value_changed) {
            *v = v_new;
            ImGui::MarkItemEdited(id);
        }

        float t = (v_max == v_min) ? 0.0f : (*v - v_min) / (v_max - v_min);
        window->DrawList->AddRectFilled(frame_bb.Min + ImVec2(0, h/2 - 3), frame_bb.Max - ImVec2(0, h/2 - 3), ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.20f, 1.0f)), 3.0f);
        window->DrawList->AddRectFilled(frame_bb.Min + ImVec2(0, h/2 - 3), ImVec2(frame_bb.Min.x + t * w, frame_bb.Max.y - h/2 + 3), ImGui::GetColorU32(ImVec4(0.40f, 0.45f, 0.95f, 1.00f)), 3.0f);
        window->DrawList->AddCircleFilled(ImVec2(frame_bb.Min.x + t * w, frame_bb.Min.y + h/2), 6.0f, ImGui::GetColorU32(ImVec4(1,1,1,1)));

        if (label_size.x > 0.0f)
            ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + (h - label_size.y) * 0.5f), label);

        return value_changed;
    }
}

void RenderMenu() {
    if (!Settings::menu_visible) return;

    ApplyMenuStyle();

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("L U M E N   E X T E R N A L", &Settings::menu_visible, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) 
    {
        ImGui::End();
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    // Horizontal top border line
    draw->AddLine(ImVec2(pos.x, pos.y + 30), ImVec2(pos.x + size.x, pos.y + 30), ImColor(50, 50, 70, 255), 1.0f);
    
    // Header text
    draw->AddText(ImVec2(pos.x + 10, pos.y + 8), GetRainbow(1.0f), "L U M E N   E X T E R N A L   V 3 . 0");
    
    // Welcome message
    const char* user_name = "[user]";
    float welcome_w = ImGui::CalcTextSize("welcome back ").x;
    float user_w = ImGui::CalcTextSize(user_name).x;
    draw->AddText(ImVec2(pos.x + size.x - welcome_w - user_w - 10, pos.y + 8), ImColor(150, 150, 160), "welcome back ");
    draw->AddText(ImVec2(pos.x + size.x - user_w - 10, pos.y + 8), ImColor(80, 110, 240), user_name);

    ImGui::SetCursorPosY(31);

    static int tab = 0;
    
    ImGui::BeginChild("##Sidebar", ImVec2(60, 0), true, ImGuiWindowFlags_NoScrollbar); {
        const char* tabIcons[] = { (const char*)u8"\ue9b3", (const char*)u8"\ue9ce", (const char*)u8"\ue994" };

        for (int i = 0; i < 3; i++) {
            bool active = (tab == i);
            ImGui::PushID(i);
            ImGui::SetCursorPos(ImVec2(10.0f, 20.0f + (float)i * 65.0f));
            if (ImGui::InvisibleButton("##TabBtn", ImVec2(40, 40))) {
                tab = i;
            }
            
            bool hovered = ImGui::IsItemHovered();
            if (active || hovered) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.45f, 0.95f, 1.00f));
            else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.65f, 1.00f));
            
            ImGui::SetCursorPos(ImVec2(10.0f, 20.0f + (float)i * 65.0f));
            if (Settings::font_icons) ImGui::PushFont(Settings::font_icons);
            ImGui::Text(tabIcons[i]);
            if (Settings::font_icons) ImGui::PopFont();
            
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Content area
    if (ImGui::BeginChild("##MainContent", ImVec2(0, 0), false)) {
        
        ImGui::SetCursorPos(ImVec2(10, 20));
        if (ImGui::BeginChild("##PrimaryColumn", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), false)) {
            ImGui::TextDisabled("Principal Module");
            ImGui::Separator();
            ImGui::Spacing();

            if (tab == 0) { // AIMBOT
                UI::Checkbox("Master Switch", &Settings::aim_enabled);
                UI::Checkbox("Legit Mode", &Settings::target_legit);
                UI::Checkbox("Safety Lock", &Settings::safety_lock);
                UI::Checkbox("Lock Prevent", &Settings::target_lock_prevent);
                UI::Checkbox("Humanized Moves", &Settings::aimbot_humanized);
                UI::Checkbox("Vis Check", &Settings::aim_vis_check);
                UI::Checkbox("Auto Wall", &Settings::aim_autowall);
                
                ImGui::Spacing();
                ImGui::TextDisabled("WEAPON PROFILE");
                ImGui::Separator();
                const char* wTabs[] = {"Rifles","SMGs","Pistols","Snipers"};
                ImGui::SetNextItemWidth(200);
                ImGui::Combo("Category", &Settings::current_weapon_tab, wTabs, 4);
                
                auto& cfg = Settings::weapon_configs[Settings::current_weapon_tab];
                UI::Checkbox("Profile Enabled",&cfg.enabled);
                UI::SliderFloat("FOV", &cfg.fov, 1.f, 30.f);
                UI::SliderFloat("Smooth", &cfg.smooth, 1.f, 30.f);
                UI::SliderFloat("Sensitivity", &Settings::sensitivity, 0.1f, 5.0f);
                
                const char* bns[]={"Head","Neck","Chest","Stomach"};
                ImGui::SetNextItemWidth(150);
                ImGui::Combo("Hitbox", &cfg.bone, bns, 4);
                
                ImGui::Spacing();
                ImGui::TextDisabled("RECOIL CONTROL");
                ImGui::Separator();
                UI::Checkbox("Standalone RCS", &Settings::rcs_standalone);
                UI::SliderFloat("Pitch/Yaw Scale", &Settings::rcs_scale, 0.5f, 5.f);
            }
            else if (tab == 1) { // VISUALS
                UI::Checkbox("ESP Master", &Settings::esp_enabled);
                UI::Checkbox("Fake Chams (White)", &Settings::fake_chams);
                UI::Checkbox("Avatar ESP", &Settings::avatar_esp);
                UI::Checkbox("Health Bar", &Settings::health_bar);
                UI::Checkbox("Armor Bar", &Settings::armor_bar);
                UI::Checkbox("Names", &Settings::names);
                UI::Checkbox("Distance", &Settings::distance);
                UI::Checkbox("Skeleton", &Settings::skeleton);
                UI::Checkbox("Snaplines", &Settings::snaplines);
                
                ImGui::Spacing();
                
                // RGB Mode Highlight
                ImVec2 cp = ImGui::GetCursorScreenPos();
                ImU32 rb = GetRainbow(1.0f);
                ImVec4 rbV = ImGui::ColorConvertU32ToFloat4(rb);
                ImGui::GetWindowDrawList()->AddRect(ImVec2(cp.x - 2, cp.y - 2), ImVec2(cp.x + ImGui::GetContentRegionAvail().x, cp.y + 24), rb, 2.0f);
                
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
                ImGui::TextColored(rbV, "RGB Mode [ACTIVE]");
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25);
                
                ImGui::PushStyleColor(ImGuiCol_CheckMark, rbV);
                UI::Checkbox("##RGBGlobal", &Settings::rainbow_global);
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }
            else if (tab == 2) { // MISC
                UI::Checkbox("Watermark", &Settings::watermark);
                UI::Checkbox("Spectator List", &Settings::spectator_list);
                
                ImGui::Spacing();
                ImGui::TextDisabled("HITSOUNDS");
                ImGui::Separator();
                if (UI::Checkbox("Enable Hitsound", &Settings::hitsound) && !Settings::hitsound_files.empty()) {
                    ImGui::SetNextItemWidth(180);
                    if (ImGui::BeginCombo("Sound", Settings::hitsound_files[Settings::hitsound_sel].c_str())) {
                        for (int n = 0; n < (int)Settings::hitsound_files.size(); n++) {
                            bool sel = (Settings::hitsound_sel == n);
                            if (ImGui::Selectable(Settings::hitsound_files[n].c_str(), sel)) Settings::hitsound_sel = n;
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("##SecondaryColumn", ImVec2(0, 0), false)) {
            ImGui::TextDisabled("Secondary Module");
            ImGui::Separator();
            ImGui::Spacing();

            if (tab == 0) { // AIMBOT
                ImGui::TextDisabled("AIM KEYBIND");
                const char* kNames[]={"Mouse1","Mouse2","Mouse3","M4","M5","V","X","Alt","Shift","F"};
                int kVKs[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,'V','X',VK_MENU,VK_SHIFT,'F'};
                ImGui::SetNextItemWidth(120);
                if(ImGui::Combo("Target Key",&Settings::aim_key_idx,kNames,10)) Settings::aim_key=kVKs[Settings::aim_key_idx];
            }
            else if (tab == 1) { // VISUALS
                UI::Checkbox("2D Box", &Settings::box_2d);
                if (Settings::box_2d) {
                    const char* styles[] = {"Regular","Corner"};
                    ImGui::SetNextItemWidth(110);
                    ImGui::Combo("Style",&Settings::box_style,styles,2);
                    UI::Checkbox("Fill", &Settings::fill_box);
                    UI::Checkbox("Outline", &Settings::outline_box);
                    UI::Checkbox("Rainbow Box", &Settings::rainbow_box);
                }
                UI::Checkbox("3D Box", &Settings::box_3d);
                
                ImGui::Spacing();
                ImGui::TextDisabled("EFFECTS");
                ImGui::Separator();
                UI::Checkbox("Fake Glow RGB", &Settings::fake_glow_enemies);
                UI::Checkbox("Enemies Only", &Settings::esp_enemies_only);
                UI::SliderFloat("RGB Speed", &Settings::rgb_speed, 0.1f, 5.f);
                UI::SliderFloat("Glow Spread", &Settings::glow_spread, 0.5f, 3.0f);
            }
            else if (tab == 2) { // MISC
                ImGui::TextDisabled("ENVIRONMENT");
                UI::Checkbox("Grenade Predict", &Settings::grenade_prediction);
                UI::Checkbox("Bomb ESP", &Settings::bomb_esp);
                UI::Checkbox("Bomb Timer HUD", &Settings::bomb_timer_hud);
                
                ImGui::Spacing();
                UI::Checkbox("Enable Hitmarker", &Settings::hitmarker);
                if (Settings::hitmarker) {
                    UI::SliderFloat("Size", &Settings::hitmarker_size, 0.5f, 3.0f);
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Footer decoration
    draw->AddText(ImVec2(pos.x + size.x / 2 - 50, pos.y + size.y - 20), ImColor(60, 60, 70), "Lumen Software (c) 2026");

    ImGui::End();
}

int main() {
    // Allocate console for debugging if not already present
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);

    SetConsoleTitleA("Lumen External Debug Console");
    std::cout << "=========================================" << std::endl;
    std::cout << "Starting cs2 Lumen External..." << std::endl;
    std::cout << "=========================================" << std::endl;

    // Connect to CS2
    std::cout << "[*] Searching for cs2.exe..." << std::endl;
    static Memory mem("cs2.exe");
    g_Memory = &mem;
    
    std::cout << "[*] Finding modules..." << std::endl;
    g_Client = mem.GetModuleAddress("client.dll");
    g_Engine = mem.GetModuleAddress("engine2.dll");

    if (!g_Client) {
        std::cout << "[!] client.dll not found. (CS2 must be running!)" << std::endl;
    } else {
        std::cout << "[+] Found client.dll at: 0x" << std::hex << g_Client << std::dec << std::endl;
    }

    // Set executable directory and scan for hitsounds
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    Settings::exeDir = exePath;
    Settings::exeDir = Settings::exeDir.substr(0, Settings::exeDir.find_last_of("\\/"));

    std::cout << "[*] Scanning for hitsounds in: " << Settings::exeDir << "\\sound\\hit\\" << std::endl;
    WIN32_FIND_DATAA findData;
    std::string soundPattern = Settings::exeDir + "\\sound\\hit\\*.mp3";
    HANDLE hFind = FindFirstFileA(soundPattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            Settings::hitsound_files.push_back(findData.cFileName);
            std::cout << "  - Loaded sound: " << findData.cFileName << std::endl;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    if (Settings::hitsound_files.empty()) Settings::hitsound_files.push_back("No sounds found");

    // Load Offsets (Try Web first, then local JSON)
    std::cout << "[*] Loading offsets..." << std::endl;
    bool offsetsLoaded = g_Off.UpdateFromWeb();
    if (!offsetsLoaded) {
        std::cout << "[*] Web update failed, loading local: dumper offsets/output/offsets.json" << std::endl;
        offsetsLoaded = g_Off.LoadFromFile("dumper offsets/output/offsets.json");
    }

    if (offsetsLoaded) {
        std::cout << "[+] Offsets loaded successfully." << std::endl;
    } else {
        std::cout << "[!] FAILED to load any offsets." << std::endl;
    }

    if (!g_Client) {
        std::cout << "[!] client.dll not found. Start CS2 first." << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        system("pause");
        return 0;
    }

    std::cout << "[*] Starting worker threads..." << std::endl;
    // Skin changer background thread
    std::thread skinThread(SkinChangerThread);
    // Entity Reader thread
    std::thread entityThread(EntityReaderThread);
    // Aimbot thread (now includes RCS)
    std::thread aimThread(AimbotThread);
    std::cout << "[+] Threads started." << std::endl;

    std::cout << "[*] Creating overlay..." << std::endl;
    CreateOverlayWindow();
    std::cout << "[*] Initializing D3D11..." << std::endl;
    if (!InitD3D()) {
        std::cout << "[!] D3D11 init failed." << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "[+] D3D11 Initialized." << std::endl;

    // ImGui
    std::cout << "[*] Setting up ImGui..." << std::endl;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.f; style.FrameRounding = 5.f;

    // Load Fonts structure
    std::cout << "[*] Loading Fonts..." << std::endl;
    
    std::cout << "[*] Loading Inter Font (Static Bold)..." << std::endl;
    Settings::font_main = io.Fonts->AddFontFromFileTTF("c:\\Users\\hoff\\OneDrive\\Área de Trabalho\\Font\\static\\Inter_18pt-Bold.ttf", 16.0f);
    if (!Settings::font_main) {
        std::cout << "[!] Failed to load static Inter font! (Using default ImGui font)" << std::endl;
        std::cout << "[?] Check path: c:\\Users\\hoff\\OneDrive\\Área de Trabalho\\Font\\static\\Inter_18pt-Bold.ttf" << std::endl;
        system("pause");
    } else {
        std::cout << "[+] Main font loaded." << std::endl;
    }

    ImFontConfig icons_config;
    icons_config.MergeMode = false;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 2;
    icons_config.OversampleV = 2;
    
    static const ImWchar icons_ranges[] = { 0xe900, 0xeb00, 0 };
    std::cout << "[*] Loading IcoMoon Font..." << std::endl;
    Settings::font_icons = io.Fonts->AddFontFromFileTTF("c:\\Users\\hoff\\OneDrive\\Área de Trabalho\\IcoMoon-Free-master\\Font\\IcoMoon-Free.ttf", 36.0f, &icons_config, icons_ranges);
    if (!Settings::font_icons) {
        std::cout << "[!] Failed to load icon font!" << std::endl;
        std::cout << "[?] Check path: c:\\Users\\hoff\\OneDrive\\Área de Trabalho\\IcoMoon-Free-master\\Font\\IcoMoon-Free.ttf" << std::endl;
        system("pause");
    } else {
        std::cout << "[+] Icon font loaded." << std::endl;
    }

    std::cout << "[*] Initializing ImGui Win32..." << std::endl;
    if (!ImGui_ImplWin32_Init(g_hWnd)) {
        std::cout << "[!] ImGui_ImplWin32_Init failed!" << std::endl;
        system("pause");
        return 1;
    }
    
    std::cout << "[*] Initializing ImGui DX11..." << std::endl;
    if (!ImGui_ImplDX11_Init(g_Device, g_Context)) {
        std::cout << "[!] ImGui_ImplDX11_Init failed!" << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "[+] ImGui backend initialized." << std::endl;

    std::cout << "[*] Initializing AvatarManager..." << std::endl;
    AvatarManager::Get().Initialize(g_Device);
    
    std::cout << "=========================================" << std::endl;
    std::cout << "[+] SETUP COMPLETE. Press any key to start rendering..." << std::endl;
    std::cout << "=========================================" << std::endl;
    system("pause");

    MSG msg;
    bool firstFrame = true;
    while (true) {
        if (firstFrame) {
            std::cout << "[*] Entering first frame..." << std::endl;
            firstFrame = false;
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) goto cleanup;
        }

        // Toggle menu
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            Settings::menu_visible = !Settings::menu_visible;
            std::cout << "[*] Menu status: " << (Settings::menu_visible ? "VISIBLE" : "HIDDEN") << std::endl;
            
            // Apply transparency only on change
            LONG ex = GetWindowLong(g_hWnd, GWL_EXSTYLE);
            SetWindowLong(g_hWnd, GWL_EXSTYLE,
                Settings::menu_visible ? (ex & ~WS_EX_TRANSPARENT) : (ex | WS_EX_TRANSPARENT));
        }

        if (!g_Device || !g_Context || !g_SwapChain || !g_RTV) {
            std::cout << "[!] D3D11 Objects lost!" << std::endl;
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawESP();
        DrawBombInfo();
        DrawBombESP();
        DrawSpectatorList();

        RenderMenu();

        ImGui::Render();
        const float clear[4] = { 0,0,0,0 };
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        g_Context->ClearRenderTargetView(g_RTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        HRESULT hr = g_SwapChain->Present(1, 0);
        if (FAILED(hr)) {
            std::cout << "[!] SwapChain Present failed: 0x" << std::hex << hr << std::dec << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

cleanup:
    g_Running = false;
    if (skinThread.joinable()) skinThread.join();
    if (entityThread.joinable()) entityThread.join();
    if (aimThread.joinable()) aimThread.join();

    AvatarManager::Get().Stop();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_RTV)      g_RTV->Release();
    if (g_SwapChain) g_SwapChain->Release();
    if (g_Context)  g_Context->Release();
    if (g_Device)   g_Device->Release();
    DestroyWindow(g_hWnd);
    return 0;
}