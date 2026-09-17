#include <array>
#include <iostream>
#include "projection.hpp"
int main() {
  constexpr int width=1280, height=720;
  const std::array<float,16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  const std::array<ucf::Vec3,3> fixture{{{-0.35F,0.55F,0.F},{0.40F,-0.45F,0.F},{0.F,0.F,0.F}}};
  std::cout << "UCF 离线投影校准：" << width << "x" << height << "\n";
  for (const auto& point: fixture) {
    const auto screen=ucf::world_to_screen(point,identity,width,height);
    std::cout << "world=("<<point.x<<','<<point.y<<','<<point.z<<") -> screen=("<<screen->x<<','<<screen->y<<")\n";
  }
}
