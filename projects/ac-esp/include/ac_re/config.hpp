#pragma once

#include <filesystem>
#include <string>

namespace ac_re {

struct AppConfig {
    int window_width{1280};
    int window_height{720};
    int max_entities{32};
    bool show_menu{true};
    bool show_diagnostics{true};
    std::string theme{"cyan"};
};

AppConfig load_config(const std::filesystem::path& path);
void write_default_config(const std::filesystem::path& path);

}  // namespace ac_re
