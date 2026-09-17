#include "ac_re/config.hpp"

#include <fstream>
#include <sstream>

namespace ac_re {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool to_bool(const std::string& value, bool fallback) {
    if (value == "true") return true;
    if (value == "false") return false;
    return fallback;
}

}  // namespace

void write_default_config(const std::filesystem::path& path) {
    std::ofstream output(path);
    output << "# AssaultCube-RE 离线演示配置\n"
           << "window_width = 1280\nwindow_height = 720\nmax_entities = 32\n"
           << "show_menu = true\nshow_diagnostics = true\ntheme = cyan\n";
}

AppConfig load_config(const std::filesystem::path& path) {
    AppConfig config;
    if (!std::filesystem::exists(path)) {
        write_default_config(path);
        return config;
    }

    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos || line.starts_with('#')) continue;
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        try {
            if (key == "window_width") config.window_width = std::stoi(value);
            else if (key == "window_height") config.window_height = std::stoi(value);
            else if (key == "max_entities") config.max_entities = std::stoi(value);
            else if (key == "show_menu") config.show_menu = to_bool(value, config.show_menu);
            else if (key == "show_diagnostics") config.show_diagnostics = to_bool(value, config.show_diagnostics);
            else if (key == "theme") config.theme = value;
        } catch (const std::exception&) {
            // 保留默认值；配置错误会在诊断输出中通过结果表现出来。
        }
    }
    return config;
}

}  // namespace ac_re
