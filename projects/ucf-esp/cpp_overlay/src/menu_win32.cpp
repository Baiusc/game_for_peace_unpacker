#include "menu_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <windows.h>

namespace ucf {

namespace {
// 上一帧菜单窗口矩形（主循环用它判定光标是否悬停菜单，动态移除点击穿透）
MenuRect g_menu_rect;
}

MenuRect query_menu_rect() { return g_menu_rect; }

void draw_menu(Settings& s, bool& show_menu, const OverlayStatus& st) {
    if (!show_menu) { g_menu_rect.valid = false; return; }

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::Begin("UCF Overlay", &show_menu,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // 缓存矩形供 update_click_through() 命中判定（有一帧延迟，可接受）
    const ImVec2 p  = ImGui::GetWindowPos();
    const ImVec2 sz = ImGui::GetWindowSize();
    g_menu_rect = {true, p.x, p.y, sz.x, sz.y};

    // 注意：ImGui 默认字体不含 CJK 字形，中文会渲染成 '?'，标签一律用 ASCII。
    ImGui::Checkbox("ESP Draw (DEL)", &s.esp_visible);
    ImGui::Checkbox("ESP Master",     &s.esp_enabled);
    ImGui::Separator();
    ImGui::Checkbox("Local",    &s.show_local);
    ImGui::Checkbox("Teammate", &s.show_teammate);
    ImGui::Checkbox("Enemy",    &s.show_enemy);
    ImGui::Checkbox("Health",   &s.show_health);
    ImGui::Checkbox("Distance", &s.show_distance);
    ImGui::Separator();
    ImGui::SliderFloat("FOV", &s.fov_deg, 10.0f, 180.0f);
    ImGui::SliderFloat("Smoothing", &s.responsiveness, 0.05f, 1.0f);
    ImGui::Combo("Target", &s.target_mode, "Nearest\0Lowest HP\0");
    ImGui::ColorEdit3("Local color",    s.color_local);
    ImGui::ColorEdit3("Teammate color", s.color_teammate);
    ImGui::ColorEdit3("Enemy color",    s.color_enemy);
    ImGui::Separator();
    if (ImGui::Button("Save Config")) save_settings(s, "ucf_overlay.ini");
    ImGui::SameLine();
    if (ImGui::Button("Load Config")) load_settings(s, "ucf_overlay.ini");
    ImGui::Text("HOME: menu   DEL: esp draw");
    ImGui::Text("mode:%s src:%s players:%d scale:%.2f",
                st.flip ? "FLIP" : "BLT", st.shm ? "shm" : "synth",
                st.players, st.scale);
    ImGui::Text("present: 0x%08lX", st.present_hr);
    ImGui::End();

    if (!show_menu) g_menu_rect.valid = false;   // 点了右上角 X 关闭
}

} // namespace ucf
#endif
