#pragma once

#include <array>
#include <optional>

#include "ac_re/model.hpp"

namespace ac_re {

std::optional<Vec2> world_to_screen(
    const Vec3& world_position,
    const std::array<float, 16>& view_matrix,
    int window_width,
    int window_height);

std::optional<Rect> project_entity(
    const Entity& entity,
    const std::array<float, 16>& view_matrix,
    int window_width,
    int window_height);

}  // namespace ac_re
