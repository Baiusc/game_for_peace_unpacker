#pragma once
#include "../sdk/game.h"
#include <string>
#include <vector>

namespace BspParser {
struct Triangle {
  Vector3 v0, v1, v2;
  float min_x, max_x;
  float min_y, max_y;
  float min_z, max_z;
};

extern std::vector<Triangle> map_triangles;
extern bool is_map_loaded;
extern char map_name_loaded[128];

bool LoadVPhysData(const std::string &vphys_data);
bool IsVisible(const Vector3 &start, const Vector3 &end);
bool IntersectRayTriangle(const Vector3 &orig, const Vector3 &dir, float &t,
                          const Triangle &tri);
float GetPenetrationDepth(const Vector3 &start, const Vector3 &end);
} // namespace BspParser
