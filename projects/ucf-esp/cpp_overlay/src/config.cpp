#include "config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ucf {

bool save_settings(const Settings& s, const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    fprintf(f, "esp_enabled=%d\n",     s.esp_enabled ? 1 : 0);
    fprintf(f, "show_local=%d\n",     s.show_local ? 1 : 0);
    fprintf(f, "show_teammate=%d\n",  s.show_teammate ? 1 : 0);
    fprintf(f, "show_enemy=%d\n",     s.show_enemy ? 1 : 0);
    fprintf(f, "show_health=%d\n",    s.show_health ? 1 : 0);
    fprintf(f, "show_distance=%d\n",  s.show_distance ? 1 : 0);
    fprintf(f, "fov_deg=%.3f\n",      s.fov_deg);
    fprintf(f, "target_mode=%d\n",    s.target_mode);
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
        else if (!std::strcmp(key, "show_local"))     s.show_local     = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_teammate"))  s.show_teammate  = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_enemy"))     s.show_enemy     = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_health"))    s.show_health    = std::atoi(val) != 0;
        else if (!std::strcmp(key, "show_distance"))  s.show_distance  = std::atoi(val) != 0;
        else if (!std::strcmp(key, "fov_deg"))        s.fov_deg        = std::atof(val);
        else if (!std::strcmp(key, "target_mode"))    s.target_mode    = std::atoi(val);
        else if (!std::strcmp(key, "responsiveness")) s.responsiveness = std::atof(val);
        else if (!std::strcmp(key, "menu_hotkey"))   s.menu_hotkey    = std::atoi(val);
        else if (!std::strcmp(key, "esp_hotkey"))     s.esp_hotkey     = std::atoi(val);
        else if (!std::strcmp(key, "esp_visible"))    s.esp_visible    = std::atoi(val) != 0;
        else if (!std::strcmp(key, "color_local"))    std::sscanf(val, "%f,%f,%f", &s.color_local[0], &s.color_local[1], &s.color_local[2]);
        else if (!std::strcmp(key, "color_teammate")) std::sscanf(val, "%f,%f,%f", &s.color_teammate[0], &s.color_teammate[1], &s.color_teammate[2]);
        else if (!std::strcmp(key, "color_enemy"))    std::sscanf(val, "%f,%f,%f", &s.color_enemy[0], &s.color_enemy[1], &s.color_enemy[2]);
    }
    std::fclose(f);
    return true;
}

} // namespace ucf
