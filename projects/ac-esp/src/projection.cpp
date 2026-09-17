#include "ac_re/projection.hpp"

namespace ac_re {

std::optional<Vec2> world_to_screen(
    const Vec3& world_position,
    const std::array<float, 16>& matrix,
    const int window_width,
    const int window_height) {
    const float w = world_position.x * matrix[3] + world_position.y * matrix[7]
        + world_position.z * matrix[11] + matrix[15];
    if (w < 0.001F) return std::nullopt;

    const float x = world_position.x * matrix[0] + world_position.y * matrix[4]
        + world_position.z * matrix[8] + matrix[12];
    const float y = world_position.x * matrix[1] + world_position.y * matrix[5]
        + world_position.z * matrix[9] + matrix[13];
    const float center_x = static_cast<float>(window_width) / 2.F;
    const float center_y = static_cast<float>(window_height) / 2.F;
    return Vec2{center_x + center_x * (x / w), center_y - center_y * (y / w)};
}

std::optional<Rect> project_entity(
    const Entity& entity,
    const std::array<float, 16>& matrix,
    const int window_width,
    const int window_height) {
    if (entity.health <= 0) return std::nullopt;
    const auto head = world_to_screen(entity.head, matrix, window_width, window_height);
    const auto feet = world_to_screen(entity.feet, matrix, window_width, window_height);
    if (!head || !feet) return std::nullopt;

    const int height = static_cast<int>(feet->y - head->y);
    if (height <= 0) return std::nullopt;
    const int width = height / 2;
    return Rect{static_cast<int>(head->x) - width / 2, static_cast<int>(head->y),
                static_cast<int>(head->x) + width / 2, static_cast<int>(head->y) + height};
}

}  // namespace ac_re
