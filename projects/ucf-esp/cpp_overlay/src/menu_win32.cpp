#include "menu_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace ucf {

namespace {
// 上一帧菜单窗口矩形（主循环用它判定光标是否悬停菜单，动态移除点击穿透）
MenuRect g_menu_rect;
}

MenuRect query_menu_rect() { return g_menu_rect; }

static void draw_log_tail(bool paused) {
    static std::vector<char> log_buffer(8192, 0);
    static int refresh_counter = 0;
    if (!paused && (refresh_counter++ % 30 == 0)) {
        std::vector<std::string> lines;
        FILE* f = std::fopen("ucf_debug.log", "r");
        if (f) {
            char line[512];
            while (std::fgets(line, sizeof(line), f)) lines.emplace_back(line);
            std::fclose(f);
            const size_t begin = lines.size() > 24 ? lines.size() - 24 : 0;
            std::string joined;
            for (size_t i = begin; i < lines.size(); ++i) joined += lines[i];
            std::memset(log_buffer.data(), 0, log_buffer.size());
            std::strncpy(log_buffer.data(), joined.c_str(), log_buffer.size() - 1);
        }
    }
    ImGui::BeginChild("debug-log", ImVec2(620, 150), true);
    if (paused) {
        ImGui::TextUnformatted("日志滚动已暂停");
    } else if (log_buffer[0] == 0) {
        ImGui::TextUnformatted("ucf_debug.log 尚未生成");
    } else {
        ImGui::InputTextMultiline("##debug-log-text", log_buffer.data(), log_buffer.size(),
                                  ImVec2(-1, 130), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::EndChild();
}

void draw_menu(Settings& s, bool& show_menu, bool& request_exit,
               bool& request_record_start, bool& request_record_stop,
               const OverlayStatus& st) {
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
        ImGui::Checkbox("显示 FOV 圈", &s.show_fov_circle);
        ImGui::SliderFloat("最大距离", &s.max_distance, 0.0f, 1000.0f, "%.0f m");
        ImGui::SliderFloat("线宽", &s.line_thickness, 1.0f, 5.0f, "%.1f");
        ImGui::TreePop();
    }
    ImGui::Separator();
    if (ImGui::TreeNode("Aimbot（仅输出角度）")) {
        ImGui::Checkbox("Aimbot 开关", &s.aimbot_enabled);
        ImGui::SliderFloat("瞄准 FOV", &s.fov_deg, 5.0f, 180.0f);
        ImGui::Checkbox("显示目标射线", &s.show_target_ray);
        ImGui::Checkbox("射线从屏幕底部", &s.ray_from_bottom);
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
    if (ImGui::TreeNode("DEV 开发者")) {
        const bool was_recording = s.dev_record_enabled;
        ImGui::Checkbox("录制真实帧", &s.dev_record_enabled);
        if (s.dev_record_enabled != was_recording) {
            if (s.dev_record_enabled) request_record_start = true;
            else request_record_stop = true;
        }
        ImGui::InputText("录制文件", s.dev_record_path, sizeof(s.dev_record_path));
        ImGui::InputInt("最大帧数(0=不限)", &s.dev_record_max_frames);
        ImGui::InputInt("每N帧录制", &s.dev_record_every);
        ImGui::InputFloat("最大时长(秒,0=不限)", &s.dev_record_duration);
        ImGui::Checkbox("录制骨骼", &s.dev_record_bones);
        ImGui::Checkbox("录制名称", &s.dev_record_name);
        ImGui::InputFloat("回放速度", &s.dev_replay_speed, 0.1f, 1.0f, "%.2fx");
        if (ImGui::Button("开始录制")) { s.dev_record_enabled = true; request_record_start = true; }
        ImGui::SameLine();
        if (ImGui::Button("停止录制")) { s.dev_record_enabled = false; request_record_stop = true; }
        ImGui::Text("状态:%s 帧数:%d 文件:%s", st.recording ? "录制中" : "停止",
                    st.recorded_frames, s.dev_record_path);
        ImGui::TextUnformatted("回放/评估：使用 tools/replay.py 的 JSONL 离线流程");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("DEBUG 调试")) {
        ImGui::Checkbox("显示日志流", &s.debug_show_log);
        ImGui::Checkbox("暂停日志滚动", &s.debug_log_paused);
        ImGui::Checkbox("显示投影数据", &s.debug_show_projection);
        ImGui::Checkbox("显示骨骼数据", &s.debug_show_bones);
        ImGui::Checkbox("显示角度平滑", &s.debug_show_angles);
        ImGui::Checkbox("显示性能", &s.debug_show_performance);
        ImGui::Checkbox("显示ESP调试标注", &s.debug_show_annotations);
        if (s.debug_show_log) draw_log_tail(s.debug_log_paused);
        if (s.debug_show_projection)
            ImGui::Text("frame:%d players:%d bones:%d/%d", st.frame_index, st.players,
                        st.bones_valid, st.bones_total);
        if (s.debug_show_bones)
            ImGui::Text("骨骼有效率: %d/%d", st.bones_valid, st.bones_total);
        if (s.debug_show_angles)
            ImGui::Text("当前目标:%d yaw=%.3f pitch=%.3f delta=%.3f",
                        st.target, st.target_yaw, st.target_pitch, st.target_delta);
        if (s.debug_show_performance)
            ImGui::Text("耗时 read=%.2fms project=%.2fms draw=%.2fms FPS=%.1f",
                        st.read_ms, st.project_ms, st.draw_ms, ImGui::GetIO().Framerate);
        ImGui::TreePop();
    }
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

    if (std::strcmp(before.dev_record_path, s.dev_record_path) != 0) {
        request_record_stop = true;
        if (s.dev_record_enabled) request_record_start = true;
    }

    if (!show_menu) g_menu_rect.valid = false;   // 点了右上角 X 关闭
}

} // namespace ucf
#endif
