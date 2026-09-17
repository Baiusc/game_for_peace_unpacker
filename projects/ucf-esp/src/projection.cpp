#include "projection.hpp"
namespace ucf {
std::optional<Vec2> world_to_screen(const Vec3& p, const std::array<float,16>& m, int width, int height) {
  const float w=p.x*m[3]+p.y*m[7]+p.z*m[11]+m[15];
  if (w <= 0.001F) return std::nullopt;
  const float x=p.x*m[0]+p.y*m[4]+p.z*m[8]+m[12];
  const float y=p.x*m[1]+p.y*m[5]+p.z*m[9]+m[13];
  return Vec2{(x/w*0.5F+0.5F)*width, (1.F-(y/w*0.5F+0.5F))*height};
}
}
