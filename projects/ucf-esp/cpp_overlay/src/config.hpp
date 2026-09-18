#pragma once

// 叠加层运行时配置（不依赖 JSON 库，用最小 key=value 文本持久化）。
// 菜单改完后 save_settings() 落盘，下次启动 load_settings() 还原。
namespace ucf {

struct Settings {
    bool  esp_enabled     = true;
    bool  show_local      = false;
    bool  show_teammate   = true;
    bool  show_enemy      = true;
    bool  show_health     = true;
    bool  show_distance   = true;
    float fov_deg         = 90.0f;
    int   target_mode     = 0;          // 0=最近, 1=最低血量
    float responsiveness  = 0.35f;      // 平滑系数（0..1]
    int   menu_hotkey     = 0x24;      // VK_HOME：切换菜单显示（原 INSERT 与注入键冲突）
    int   esp_hotkey      = 0x2E;      // VK_DELETE：切换 ESP 绘制层显隐
    bool  esp_visible     = false;     // ESP 绘制层显隐（会话级，默认隐藏）
    float color_local[3]     = {0.22f, 0.83f, 0.33f};
    float color_teammate[3]  = {0.35f, 0.65f, 1.00f};
    float color_enemy[3]     = {0.97f, 0.32f, 0.29f};
};

bool load_settings(Settings& s, const char* path);
bool save_settings(const Settings& s, const char* path);

} // namespace ucf
