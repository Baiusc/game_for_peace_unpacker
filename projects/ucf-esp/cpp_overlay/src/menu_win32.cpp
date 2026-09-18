#include "menu_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <windows.h>
#include <cstring>

namespace ucf {

namespace {
// 上一帧菜单窗口矩形（主循环用它判定光标是否悬停菜单，动态移除点击穿透）
MenuRect g_menu_rect;
}

MenuRect query_menu_rect() { return g_menu_rect; }

void draw_menu(Settings& s, bool& show_menu, bool& request_exit, const OverlayStatus& st) {
    if (!show_menu) { g_menu_rect.valid = false; return; }

    const Settings before = s;

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::Begin("UCF Overlay", &show_menu,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // 缓存矩形供 update_click_through() 命中判定（有一帧延迟，可接受）
    const ImVec2 p  = ImGui::GetWindowPos();
    const ImVec2 sz = ImGui::GetWindowSize();
    g_menu_rect = {true, p.x, p.y, sz.x, sz.y};

    // 中文渲染依赖 main 里 init_overlay_fonts() 加载的系统 CJK 字体；缺失则退回 ASCII。
    ImGui::TextUnformatted("UCF 调试可视化");
    ImGui::Checkbox("ESP 绘制层（DEL）", &s.esp_visible);
    ImGui::Checkbox("ESP 总开关",        &s.esp_enabled);
    if (ImGui::TreeNode("ESP 显示项")) {
        ImGui::Checkbox("显示方框", &s.show_box);
        ImGui::Checkbox("显示骨骼", &s.show_skeleton);
        ImGui::Checkbox("本地玩家", &s.show_local);
        ImGui::Checkbox("队友",     &s.show_teammate);
        ImGui::Checkbox("敌人",     &s.show_enemy);
        ImGui::Checkbox("血条",     &s.show_health);
        ImGui::Checkbox("距离",     &s.show_distance);
        ImGui::SliderFloat("最大距离", &s.max_distance, 0.0f, 1000.0f, "%.0f m");
        ImGui::SliderFloat("线宽", &s.line_thickness, 1.0f, 5.0f, "%.1f");
        ImGui::TreePop();
    }
    ImGui::Separator();
    if (ImGui::TreeNode("Aimbot（仅输出角度）")) {
        ImGui::Checkbox("Aimbot 开关", &s.aimbot_enabled);
        ImGui::SliderFloat("瞄准 FOV", &s.fov_deg, 5.0f, 180.0f);
        ImGui::SliderFloat("瞄准最大距离", &s.aim_max_distance, 0.0f, 1000.0f, "%.0f m");
        ImGui::Combo("目标选择", &s.target_mode, "最近目标\0最低血量\0准星最近\0");
        ImGui::Checkbox("允许队友", &s.aim_teammates);
        ImGui::Checkbox("允许死亡目标", &s.aim_dead);
        ImGui::Checkbox("仅屏幕内目标", &s.aim_visible_only);
        ImGui::TreePop();
    }
    ImGui::SliderFloat("平滑系数",   &s.responsiveness, 0.05f, 1.0f);
    ImGui::ColorEdit3("本地颜色",   s.color_local);
    ImGui::ColorEdit3("队友颜色",   s.color_teammate);
    ImGui::ColorEdit3("敌人颜色",   s.color_enemy);
    ImGui::Separator();
    if (ImGui::TreeNode("退出设置")) {
        ImGui::Checkbox("退出时删除配置 (ucf_overlay.ini)", &s.exit_delete_config);
        ImGui::Checkbox("退出时删除日志 (ucf_debug.log)", &s.exit_delete_log);
        if (ImGui::Button("退出程序 (END)")) request_exit = true;
        ImGui::TreePop();
    }
    if (ImGui::Button("保存配置")) save_settings(s, "ucf_overlay.ini");
    ImGui::SameLine();
    if (ImGui::Button("加载配置")) load_settings(s, "ucf_overlay.ini");
    ImGui::Text("HOME: 菜单    DEL: 绘制层");
    ImGui::Text("模式:%s 源:%s 玩家:%d 缩放:%.2f FPS:%.1f",
                st.flip ? "FLIP" : "BLT", st.shm ? "shm" : "synth",
                st.players, st.scale, ImGui::GetIO().Framerate);
    ImGui::Text("present: 0x%08lX", st.present_hr);
    ImGui::Text("target:%d yaw:%.3f pitch:%.3f", st.target, st.target_yaw, st.target_pitch);
    ImGui::End();

    // 配置项在菜单中修改后立即持久化；按钮仍保留给用户显式保存。
    if (std::memcmp(&before, &s, sizeof(Settings)) != 0)
        save_settings(s, "ucf_overlay.ini");

    if (!show_menu) g_menu_rect.valid = false;   // 点了右上角 X 关闭
}

} // namespace ucf
#endif
