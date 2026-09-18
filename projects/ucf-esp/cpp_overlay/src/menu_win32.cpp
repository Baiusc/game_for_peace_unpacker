#include "menu_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <windows.h>

namespace ucf {

void draw_menu(Settings& s, bool& show_menu) {
    // INSERT 切换菜单显隐（轮询，无回调、无溢出风险）
    static bool insert_down = false;
    bool now = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (now && !insert_down) show_menu = !show_menu;
    insert_down = now;

    if (!show_menu) return;

    ImGui::Begin("UCF Overlay", &show_menu, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("ESP 总开关", &s.esp_enabled);
    ImGui::Separator();
    ImGui::Checkbox("本地玩家", &s.show_local);
    ImGui::Checkbox("队友",     &s.show_teammate);
    ImGui::Checkbox("敌人",     &s.show_enemy);
    ImGui::Checkbox("血条",     &s.show_health);
    ImGui::Checkbox("距离",     &s.show_distance);
    ImGui::Separator();
    ImGui::SliderFloat("FOV", &s.fov_deg, 10.0f, 180.0f);
    ImGui::SliderFloat("平滑系数", &s.responsiveness, 0.05f, 1.0f);
    ImGui::Combo("目标选择", &s.target_mode, "最近\0最低血量\0");
    ImGui::ColorEdit3("本地颜色", s.color_local);
    ImGui::ColorEdit3("队友颜色", s.color_teammate);
    ImGui::ColorEdit3("敌人颜色", s.color_enemy);
    ImGui::Separator();
    if (ImGui::Button("保存配置")) save_settings(s, "ucf_overlay.ini");
    ImGui::SameLine();
    if (ImGui::Button("载入配置")) load_settings(s, "ucf_overlay.ini");
    ImGui::Text("按 INSERT 隐藏 / 显示菜单");
    ImGui::End();
}

} // namespace ucf
#endif
