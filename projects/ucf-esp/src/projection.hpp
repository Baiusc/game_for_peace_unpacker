#pragma once
#include <array>
#include <optional>
namespace ucf {
struct Vec2 { float x; float y; };
struct Vec3 { float x; float y; float z; };
std::optional<Vec2> world_to_screen(const Vec3& point, const std::array<float,16>& m, int width, int height);
}
