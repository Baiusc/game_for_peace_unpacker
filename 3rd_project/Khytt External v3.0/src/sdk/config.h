#pragma once
#include <string>

namespace Settings {
inline bool visual_esp = true;
inline bool visual_team_check = true;
inline bool visual_hp_bar = false;
inline bool visual_box = false;
inline bool visual_chams = false;
inline bool visual_names = false;
inline bool visual_tracers = false;
inline bool visual_skeleton = false;
inline bool visual_weapon = false;
inline bool visual_ammo = false;

inline bool rcs_enabled = true;
inline bool rcs_auto = true;
inline float rcs_scale_x = 2.0f;
inline float rcs_scale_y = 2.0f;
inline float rcs_smooth = 5.0f;

inline bool visual_radar = false;
inline bool radar_rotating = false;
inline float radar_pos_x = 5.0f;
inline float radar_pos_y = 5.0f;
inline float radar_size = 150.0f;
inline float radar_scale = 10.0f;

inline float sensitivity = 1.0f;

inline bool visual_offscreen = false;
inline bool visual_crosshair = false;
inline float visual_crosshair_size = 5.0f;
inline float visual_crosshair_color[3] = {1.0f, 0.0f, 0.0f};

inline float ui_theme_color[3] = {0.6f, 0.4f, 1.0f};
inline float ui_transparency = 0.95f;

inline bool visual_c4 = false;

inline float visual_box_color[3] = {0.0f, 1.0f, 0.0f}; // Default Green Visible
inline float visual_box_color_hidden[3] = {1.0f, 0.0f,
                                           0.0f}; // Default Red Hidden
inline float visual_skeleton_color[3] = {1.0f, 1.0f, 1.0f};
inline float visual_skeleton_color_hidden[3] = {0.5f, 0.5f, 0.5f};
inline float visual_chams_color[3] = {1.0f, 0.0f, 1.0f};
inline float visual_tracers_color[3] = {1.0f, 1.0f, 0.0f};

struct WeaponSettings {
  bool enabled = true;
  float fov = 10.0f;
  float smooth = 5.0f;
  int bone = 0; // 0=Head, 1=Neck, 2=Chest, 3=Stomach
};

inline WeaponSettings weapon_configs[4];
inline int current_weapon_type = 0;
inline bool aim_auto_tab = true;
inline int detected_item_idx = 0;
inline std::string detected_weapon_name = "None";

inline bool target_enabled = false;
inline bool target_legit = false;
inline bool target_visible_check = false;
inline bool target_smoothing = true;
inline float target_fov_range = 10.0f;
inline float target_smooth_factor = 5.0f;
inline int target_bone_idx = 0;
inline bool draw_fov_circle = false;
inline bool target_lock_prevent = false;
inline bool lock_prevent_active = false;
inline bool aimbot_humanized = false;
inline float aimbot_jitter_scale = 0.5f;

inline bool trigger_active = false;
inline int trigger_reaction_ms = 0;

inline int target_key_code = 0x06;
inline int target_key_idx = 4;

inline bool no_spread_toggle = false;

inline bool draw_watermark = false;
inline bool request_exit = false;
inline bool auto_hop = false;
inline bool draw_spectators = false;

inline bool safety_lock = false;

inline bool menu_visible = true;
} // namespace Settings
