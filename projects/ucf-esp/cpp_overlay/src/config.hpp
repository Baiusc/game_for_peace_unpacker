#pragma once

// 叠加层运行时配置（不依赖 JSON 库，用最小 key=value 文本持久化）。
// 菜单改完后 save_settings() 落盘，下次启动 load_settings() 还原。
namespace ucf {

struct Settings {
    bool  esp_enabled     = true;
    bool  show_box        = true;
    bool  show_skeleton   = false;
    bool  show_local      = false;
    bool  show_teammate   = true;
    bool  show_enemy      = true;
    bool  show_health     = true;
    bool  show_distance   = true;
    bool  show_fov_circle = false;
    float max_distance    = 250.0f;
    float line_thickness  = 2.0f;
    bool  aimbot_enabled  = false;
    bool  aim_teammates   = false;
    bool  aim_dead        = false;
    bool  aim_visible_only = true;
    float fov_deg         = 90.0f;
    bool  show_target_ray = true;
    bool  ray_from_bottom = true;
    int   aim_selection_mode = 0; // 0=屏幕空间距屏心，1=角度空间FOV
    int   aim_point_mode = 1;     // 0=身体中心，1=头，2=胸，3=指定骨骼
    int   aim_bone_id = 10;       // HumanBodyBones 槽位，默认 Head
    int   target_mode     = 0;          // 0=最近, 1=最低血量, 2=准星最近
    float aim_max_distance = 250.0f;
    float responsiveness  = 0.35f;      // 平滑系数（0..1]
    int   menu_hotkey     = 0x24;      // VK_HOME：切换菜单显示（原 INSERT 与注入键冲突）
    int   esp_hotkey      = 0x2E;      // VK_DELETE：切换 ESP 绘制层显隐
    bool  esp_visible     = false;     // ESP 绘制层显隐（会话级，默认隐藏）
    bool  exit_delete_config = false;  // 默认保留 ucf_overlay.ini
    bool  exit_delete_log = false;     // 默认保留 ucf_debug.log
    bool  dev_record_enabled = false;
    bool  dev_record_bones = true;
    bool  dev_record_name = true;
    int   dev_record_max_frames = 0;
    int   dev_record_every = 1;
    float dev_record_duration = 0.0f;
    float dev_replay_speed = 1.0f;
    char  dev_record_path[260] = "dev_frames.jsonl";
    bool  debug_show_log = true;
    bool  debug_log_paused = false;
    bool  debug_show_projection = false;
    bool  debug_show_bones = false;
    bool  debug_show_angles = true;
    bool  debug_show_performance = true;
    bool  debug_show_annotations = false;
    float color_local[3]     = {0.22f, 0.83f, 0.33f};
    float color_teammate[3]  = {0.35f, 0.65f, 1.00f};
    float color_enemy[3]     = {0.97f, 0.32f, 0.29f};
};

bool load_settings(Settings& s, const char* path);
bool save_settings(const Settings& s, const char* path);

} // namespace ucf
