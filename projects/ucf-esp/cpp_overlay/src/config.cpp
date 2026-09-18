#include "config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ucf {

bool save_settings(const Settings& s, const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    fprintf(f, "esp_enabled=%d\n",     s.esp_enabled ? 1 : 0);
    fprintf(f, "show_box=%d\n",        s.show_box ? 1 : 0);
    fprintf(f, "show_skeleton=%d\n",   s.show_skeleton ? 1 : 0);
    fprintf(f, "show_local=%d\n",     s.show_local ? 1 : 0);
    fprintf(f, "show_teammate=%d\n",  s.show_teammate ? 1 : 0);
    fprintf(f, "show_enemy=%d\n",     s.show_enemy ? 1 : 0);
    fprintf(f, "show_health=%d\n",    s.show_health ? 1 : 0);
    fprintf(f, "show_distance=%d\n",  s.show_distance ? 1 : 0);
    fprintf(f, "show_fov_circle=%d\n", s.show_fov_circle ? 1 : 0);
    fprintf(f, "max_distance=%.3f\n", s.max_distance);
    fprintf(f, "line_thickness=%.3f\n", s.line_thickness);
    fprintf(f, "aimbot_enabled=%d\n", s.aimbot_enabled ? 1 : 0);
    fprintf(f, "aim_teammates=%d\n", s.aim_teammates ? 1 : 0);
    fprintf(f, "aim_dead=%d\n", s.aim_dead ? 1 : 0);
    fprintf(f, "aim_visible_only=%d\n", s.aim_visible_only ? 1 : 0);
    fprintf(f, "aim_lock_prevent=%d\n", s.aim_lock_prevent ? 1 : 0);
    fprintf(f, "input_sim_enabled=%d\n", s.input_sim_enabled ? 1 : 0);
    fprintf(f, "input_sim_aim_key=%d\n", s.input_sim_aim_key);
    fprintf(f, "input_sim_fire_key=%d\n", s.input_sim_fire_key);
    fprintf(f, "input_sim_tolerance_px=%.3f\n", s.input_sim_tolerance_px);
    fprintf(f, "input_sim_jitter_px=%d\n", s.input_sim_jitter_px);
    fprintf(f, "fov_deg=%.3f\n",      s.fov_deg);
    fprintf(f, "show_target_ray=%d\n", s.show_target_ray ? 1 : 0);
    fprintf(f, "ray_from_bottom=%d\n", s.ray_from_bottom ? 1 : 0);
    fprintf(f, "aim_selection_mode=%d\n", s.aim_selection_mode);
    fprintf(f, "aim_point_mode=%d\n", s.aim_point_mode);
    fprintf(f, "aim_bone_id=%d\n", s.aim_bone_id);
    fprintf(f, "target_mode=%d\n",    s.target_mode);
    fprintf(f, "aim_max_distance=%.3f\n", s.aim_max_distance);
    fprintf(f, "responsiveness=%.3f\n", s.responsiveness);
    fprintf(f, "menu_hotkey=%d\n",    s.menu_hotkey);
    fprintf(f, "esp_hotkey=%d\n",     s.esp_hotkey);
    fprintf(f, "esp_visible=%d\n",    s.esp_visible ? 1 : 0);
    fprintf(f, "exit_delete_config=%d\n", s.exit_delete_config ? 1 : 0);
    fprintf(f, "exit_delete_log=%d\n", s.exit_delete_log ? 1 : 0);
    fprintf(f, "dev_record_enabled=%d\n", s.dev_record_enabled ? 1 : 0);
    fprintf(f, "dev_record_bones=%d\n", s.dev_record_bones ? 1 : 0);
    fprintf(f, "dev_record_name=%d\n", s.dev_record_name ? 1 : 0);
    fprintf(f, "dev_record_max_frames=%d\n", s.dev_record_max_frames);
    fprintf(f, "dev_record_every=%d\n", s.dev_record_every);
    fprintf(f, "dev_record_duration=%.3f\n", s.dev_record_duration);
    fprintf(f, "dev_replay_speed=%.3f\n", s.dev_replay_speed);
    fprintf(f, "dev_replay_enabled=%d\n", s.dev_replay_enabled ? 1 : 0);
    fprintf(f, "dev_record_path=%s\n", s.dev_record_path);
    fprintf(f, "debug_show_log=%d\n", s.debug_show_log ? 1 : 0);
    fprintf(f, "debug_log_paused=%d\n", s.debug_log_paused ? 1 : 0);
    fprintf(f, "debug_show_projection=%d\n", s.debug_show_projection ? 1 : 0);
    fprintf(f, "debug_show_bones=%d\n", s.debug_show_bones ? 1 : 0);
    fprintf(f, "debug_show_angles=%d\n", s.debug_show_angles ? 1 : 0);
    fprintf(f, "debug_show_performance=%d\n", s.debug_show_performance ? 1 : 0);
    fprintf(f, "debug_show_annotations=%d\n", s.debug_show_annotations ? 1 : 0);
    fprintf(f, "color_local=%.3f,%.3f,%.3f\n",
            s.color_local[0], s.color_local[1], s.color_local[2]);
    fprintf(f, "color_teammate=%.3f,%.3f,%.3f\n",
            s.color_teammate[0], s.color_teammate[1], s.color_teammate[2]);
    fprintf(f, "color_enemy=%.3f,%.3f,%.3f\n",
            s.color_enemy[0], s.color_enemy[1], s.color_enemy[2]);
    std::fclose(f);
    return true;
}

bool load_settings(Settings& s, const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        char* nl = std::strchr(line, '\n'); if (nl) *nl = 0;
        char* eq = std::strchr(line, '='); if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* val = eq + 1;
        if      (!std::strcmp(key, "esp_enabled"))    s.esp_enabled    = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_box"))       s.show_box       = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_skeleton"))  s.show_skeleton  = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_local"))     s.show_local     = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_teammate"))  s.show_teammate  = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_enemy"))     s.show_enemy     = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_health"))    s.show_health    = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_distance"))  s.show_distance  = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_fov_circle")) s.show_fov_circle = std::atoi(val) != 0;
        else if (!std::strcmp(key, "max_distance"))   s.max_distance   = std::atof(val);
        else if (!std::strcmp(key, "line_thickness")) s.line_thickness = std::atof(val);
        else if (!std::strcmp(key, "aimbot_enabled")) s.aimbot_enabled = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_teammates"))  s.aim_teammates = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_dead"))       s.aim_dead = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_visible_only")) s.aim_visible_only = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_lock_prevent")) s.aim_lock_prevent = std::atoi(val) != 0;
        else if (!std::strcmp(key, "input_sim_enabled")) s.input_sim_enabled = std::atoi(val) != 0;
        else if (!std::strcmp(key, "input_sim_aim_key")) s.input_sim_aim_key = std::atoi(val);
        else if (!std::strcmp(key, "input_sim_fire_key")) s.input_sim_fire_key = std::atoi(val);
        else if (!std::strcmp(key, "input_sim_tolerance_px")) s.input_sim_tolerance_px = std::atof(val);
        else if (!std::strcmp(key, "input_sim_jitter_px")) s.input_sim_jitter_px = std::atoi(val);
        else if (!std::strcmp(key, "fov_deg"))        s.fov_deg        = std::atof(val);
        else if (!std::strcmp(key, "show_target_ray")) s.show_target_ray = std::atoi(val) != 0;
        else if (!std::strcmp(key, "ray_from_bottom")) s.ray_from_bottom = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_selection_mode")) s.aim_selection_mode = std::atoi(val);
        else if (!std::strcmp(key, "aim_point_mode")) s.aim_point_mode = std::atoi(val);
        else if (!std::strcmp(key, "aim_bone_id")) s.aim_bone_id = std::atoi(val);
        else if (!std::strcmp(key, "target_mode"))    s.target_mode    = std::atoi(val);
        else if (!std::strcmp(key, "aim_max_distance")) s.aim_max_distance = std::atof(val);
        else if (!std::strcmp(key, "responsiveness")) s.responsiveness = std::atof(val);
        else if (!std::strcmp(key, "menu_hotkey"))   s.menu_hotkey    = std::atoi(val);
        else if (!std::strcmp(key, "esp_hotkey"))     s.esp_hotkey     = std::atoi(val);
        else if (!std::strcmp(key, "esp_visible"))    s.esp_visible    = std::atoi(val) != 0;
        else if (!std::strcmp(key, "exit_delete_config")) s.exit_delete_config = std::atoi(val) != 0;
        else if (!std::strcmp(key, "exit_delete_log")) s.exit_delete_log = std::atoi(val) != 0;
        else if (!std::strcmp(key, "dev_record_enabled")) s.dev_record_enabled = std::atoi(val) != 0;
        else if (!std::strcmp(key, "dev_record_bones")) s.dev_record_bones = std::atoi(val) != 0;
        else if (!std::strcmp(key, "dev_record_name")) s.dev_record_name = std::atoi(val) != 0;
        else if (!std::strcmp(key, "dev_record_max_frames")) s.dev_record_max_frames = std::atoi(val);
        else if (!std::strcmp(key, "dev_record_every")) s.dev_record_every = std::atoi(val);
        else if (!std::strcmp(key, "dev_record_duration")) s.dev_record_duration = std::atof(val);
        else if (!std::strcmp(key, "dev_replay_speed")) s.dev_replay_speed = std::atof(val);
        else if (!std::strcmp(key, "dev_replay_enabled")) s.dev_replay_enabled = std::atoi(val) != 0;
        else if (!std::strcmp(key, "dev_record_path")) {
            std::strncpy(s.dev_record_path, val, sizeof(s.dev_record_path) - 1);
            s.dev_record_path[sizeof(s.dev_record_path) - 1] = 0;
        }
        else if (!std::strcmp(key, "debug_show_log")) s.debug_show_log = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_log_paused")) s.debug_log_paused = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_show_projection")) s.debug_show_projection = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_show_bones")) s.debug_show_bones = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_show_angles")) s.debug_show_angles = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_show_performance")) s.debug_show_performance = std::atoi(val) != 0;
        else if (!std::strcmp(key, "debug_show_annotations")) s.debug_show_annotations = std::atoi(val) != 0;
        else if (!std::strcmp(key, "color_local"))    std::sscanf(val, "%f,%f,%f", &s.color_local[0], &s.color_local[1], &s.color_local[2]);
        else if (!std::strcmp(key, "color_teammate")) std::sscanf(val, "%f,%f,%f", &s.color_teammate[0], &s.color_teammate[1], &s.color_teammate[2]);
        else if (!std::strcmp(key, "color_enemy"))    std::sscanf(val, "%f,%f,%f", &s.color_enemy[0], &s.color_enemy[1], &s.color_enemy[2]);
    }
    std::fclose(f);
    if (s.fov_deg < 5.0f) s.fov_deg = 5.0f;
    if (s.fov_deg > 180.0f) s.fov_deg = 180.0f;
    if (s.target_mode < 0) s.target_mode = 0;
    if (s.target_mode > 2) s.target_mode = 2;
    if (s.responsiveness < 0.0f) s.responsiveness = 0.0f;
    if (s.responsiveness > 1.0f) s.responsiveness = 1.0f;
    if (s.max_distance < 0.0f) s.max_distance = 0.0f;
    if (s.aim_max_distance < 0.0f) s.aim_max_distance = 0.0f;
    if (s.input_sim_tolerance_px < 0.0f) s.input_sim_tolerance_px = 0.0f;
    if (s.input_sim_jitter_px < 0) s.input_sim_jitter_px = 0;
    if (s.dev_record_max_frames < 0) s.dev_record_max_frames = 0;
    if (s.dev_record_every < 1) s.dev_record_every = 1;
    if (s.dev_record_duration < 0.0f) s.dev_record_duration = 0.0f;
    if (s.dev_replay_speed < 0.1f) s.dev_replay_speed = 0.1f;
    if (s.aim_selection_mode < 0 || s.aim_selection_mode > 1) s.aim_selection_mode = 0;
    if (s.aim_point_mode < 0 || s.aim_point_mode > 3) s.aim_point_mode = 1;
    if (s.aim_bone_id < 0 || s.aim_bone_id >= 19) s.aim_bone_id = 11;
    return true;
}

} // namespace ucf
