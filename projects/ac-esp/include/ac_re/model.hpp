#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ac_re {

struct Vec2 { float x{}; float y{}; };
struct Vec3 { float x{}; float y{}; float z{}; };
struct Rect { int left{}; int top{}; int right{}; int bottom{}; };

struct Entity {
    std::uint32_t id{};
    std::string label;
    int health{};
    Vec3 head;
    Vec3 feet;
};

struct FrameSnapshot {
    std::array<float, 16> view_matrix{};
    std::vector<Entity> entities;
};

}  // namespace ac_re
