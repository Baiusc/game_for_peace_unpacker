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
    fprintf(f, "max_distance=%.3f\n", s.max_distance);
    fprintf(f, "line_thickness=%.3f\n", s.line_thickness);
    fprintf(f, "aimbot_enabled=%d\n", s.aimbot_enabled ? 1 : 0);
    fprintf(f, "aim_teammates=%d\n", s.aim_teammates ? 1 : 0);
    fprintf(f, "aim_dead=%d\n", s.aim_dead ? 1 : 0);
    fprintf(f, "aim_visible_only=%d\n", s.aim_visible_only ? 1 : 0);
    fprintf(f, "fov_deg=%.3f\n",      s.fov_deg);
    fprintf(f, "target_mode=%d\n",    s.target_mode);
    fprintf(f, "aim_max_distance=%.3f\n", s.aim_max_distance);
    fprintf(f, "responsiveness=%.3f\n", s.responsiveness);
    fprintf(f, "menu_hotkey=%d\n",    s.menu_hotkey);
    fprintf(f, "esp_hotkey=%d\n",     s.esp_hotkey);
    fprintf(f, "esp_visible=%d\n",    s.esp_visible ? 1 : 0);
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
        else if (!std::strcmp(key, "max_distance"))   s.max_distance   = std::atof(val);
        else if (!std::strcmp(key, "line_thickness")) s.line_thickness = std::atof(val);
        else if (!std::strcmp(key, "aimbot_enabled")) s.aimbot_enabled = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_teammates"))  s.aim_teammates = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_dead"))       s.aim_dead = std::atoi(val) != 0;
        else if (!std::strcmp(key, "aim_visible_only")) s.aim_visible_only = std::atoi(val) != 0;
        else if (!std::strcmp(key, "fov_deg"))        s.fov_deg        = std::atof(val);
        else if (!std::strcmp(key, "target_mode"))    s.target_mode    = std::atoi(val);
        else if (!std::strcmp(key, "aim_max_distance")) s.aim_max_distance = std::atof(val);
        else if (!std::strcmp(key, "responsiveness")) s.responsiveness = std::atof(val);
        else if (!std::strcmp(key, "menu_hotkey"))   s.menu_hotkey    = std::atoi(val);
        else if (!std::strcmp(key, "esp_hotkey"))     s.esp_hotkey     = std::atoi(val);
        else if (!std::strcmp(key, "esp_visible"))    s.esp_visible    = std::atoi(val) != 0;
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
    return true;
}

} // namespace ucf
