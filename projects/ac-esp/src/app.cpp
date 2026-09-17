#include "ac_re/app.hpp"

#include <algorithm>
#include <iostream>

#include "ac_re/projection.hpp"

namespace ac_re {

Application::Application(AppConfig config, const EntitySource& entity_source)
    : config_(std::move(config)), entity_source_(entity_source) {}

int Application::run_once() const {
    const auto frame = entity_source_.read_frame();
    int projected_count = 0;

    if (config_.show_menu) {
        std::cout << "[菜单] AssaultCube-RE 离线演示 | 主题=" << config_.theme
                  << " | 尺寸=" << config_.window_width << 'x' << config_.window_height << '\n';
        std::cout << "[菜单] 合成数据源=启用 | 进程读取=禁用 | 运行模式=diagnostic\n";
    }

    for (const auto& entity : frame.entities) {
        if (projected_count >= config_.max_entities) break;
        const auto rectangle = project_entity(entity, frame.view_matrix, config_.window_width, config_.window_height);
        if (!rectangle) continue;
        ++projected_count;
        std::cout << "[绘制] " << entity.label << " rect=(" << rectangle->left << ',' << rectangle->top
                  << "," << rectangle->right << ',' << rectangle->bottom << ")\n";
    }

    if (config_.show_diagnostics) {
        std::cout << "[诊断] 输入实体=" << frame.entities.size() << "，有效投影=" << projected_count
                  << "，无进程/驱动/网络访问。\n";
    }
    return 0;
}

}  // namespace ac_re
