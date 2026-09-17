#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include "projection.hpp"
int main() {
  const std::array<float,16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  auto center=ucf::world_to_screen({0,0,0},m,1280,720);
  assert(center && std::fabs(center->x-640.F)<0.01F && std::fabs(center->y-360.F)<0.01F);
  auto corner=ucf::world_to_screen({-1,1,0},m,1280,720);
  assert(corner && std::fabs(corner->x)<0.01F && std::fabs(corner->y)<0.01F);
  const std::array<float,16> behind{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,-1};
  assert(!ucf::world_to_screen({0,0,0},behind,1280,720));
  std::cout << "ucf_projection_tests: 3/3 PASS\n";
}
