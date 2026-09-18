#define _CRT_SECURE_NO_WARNINGS
#include "bsp_parser.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#undef min
#undef max

namespace BspParser {
std::vector<Triangle> map_triangles;
bool is_map_loaded = false;
char map_name_loaded[128] = "Awaiting match...";

// Helper to decode #[ hex ] blobs from cs2-phys-extractor
static std::vector<uint8_t> ParseHexBlob(const std::string &data, size_t start,
                                         size_t end) {
  std::vector<uint8_t> bytes;
  for (size_t i = start; i < end;) {
    char c = data[i];
    if (isspace(c)) {
      i++;
      continue;
    }
    if (isxdigit(c) && i + 1 < end && isxdigit(data[i + 1])) {
      std::string hexByte = data.substr(i, 2);
      bytes.push_back((uint8_t)strtol(hexByte.c_str(), NULL, 16));
      i += 2;
    } else {
      i++;
    }
  }
  return bytes;
}

bool LoadVPhysData(const std::string &vphys_data) {
  map_triangles.clear();
  is_map_loaded = false;

  size_t pos = 0;
  // Search for the binary format (cs2-phys-extractor dump)
  while ((pos = vphys_data.find("m_VertexPositions", pos)) !=
         std::string::npos) {
    size_t open_vert = vphys_data.find("#[", pos);
    if (open_vert == std::string::npos) {
      pos++;
      continue;
    }
    size_t close_vert = vphys_data.find("]", open_vert);

    size_t next_tri = vphys_data.find("m_Triangles", pos);
    size_t next_vert = vphys_data.find("m_VertexPositions", pos + 10);

    // If we found a corresponding Triangles block before the next Vertex block
    if (next_tri != std::string::npos &&
        (next_vert == std::string::npos || next_tri < next_vert)) {
      size_t open_tri = vphys_data.find("#[", next_tri);
      size_t close_tri = vphys_data.find("]", open_tri);

      if (open_tri != std::string::npos && close_tri != std::string::npos &&
          close_vert != std::string::npos) {
        std::vector<uint8_t> vertData =
            ParseHexBlob(vphys_data, open_vert + 2, close_vert);
        std::vector<uint8_t> triData =
            ParseHexBlob(vphys_data, open_tri + 2, close_tri);

        std::vector<Vector3> verts;
        for (size_t i = 0; i + 11 < vertData.size(); i += 12) {
          float x = *(float *)(&vertData[i]);
          float y = *(float *)(&vertData[i + 4]);
          float z = *(float *)(&vertData[i + 8]);
          verts.push_back({x, y, z});
        }

        std::vector<int> tris;
        for (size_t i = 0; i + 3 < triData.size(); i += 4) {
          int idx = *(int *)(&triData[i]);
          tris.push_back(idx);
        }

        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
          int i0 = tris[i];
          int i1 = tris[i + 1];
          int i2 = tris[i + 2];
          if (i0 >= 0 && i0 < verts.size() && i1 >= 0 && i1 < verts.size() &&
              i2 >= 0 && i2 < verts.size()) {
            Triangle t;
            t.v0 = verts[i0];
            t.v1 = verts[i1];
            t.v2 = verts[i2];
            t.min_x = std::min({t.v0.x, t.v1.x, t.v2.x});
            t.max_x = std::max({t.v0.x, t.v1.x, t.v2.x});
            t.min_y = std::min({t.v0.y, t.v1.y, t.v2.y});
            t.max_y = std::max({t.v0.y, t.v1.y, t.v2.y});
            t.min_z = std::min({t.v0.z, t.v1.z, t.v2.z});
            t.max_z = std::max({t.v0.z, t.v1.z, t.v2.z});
            map_triangles.push_back(t);
          }
        }
      }
    }
    pos++;
  }

  // Fallback to the old generic text float format [ %f, %f, %f ]
  if (map_triangles.empty()) {
    std::vector<Vector3> parsed_positions;
    std::vector<int> parsed_indices;

    size_t pos_idx = vphys_data.find("m_vPos");
    if (pos_idx != std::string::npos) {
      size_t open_bracket = vphys_data.find("[", pos_idx);
      size_t close_bracket = vphys_data.find("]", open_bracket);
      if (open_bracket != std::string::npos &&
          close_bracket != std::string::npos) {
        std::string block =
            vphys_data.substr(open_bracket, close_bracket - open_bracket);
        size_t p = 0;
        while ((p = block.find("[", p)) != std::string::npos) {
          float x = 0, y = 0, z = 0;
          if (sscanf(block.c_str() + p, "[ %f, %f, %f ]", &x, &y, &z) == 3) {
            parsed_positions.push_back({x, y, z});
          }
          p++;
        }
      }
    }

    size_t tri_idx = vphys_data.find("m_nTriangles");
    if (tri_idx == std::string::npos)
      tri_idx = vphys_data.find("m_nIndex");

    if (tri_idx != std::string::npos) {
      size_t open_bracket = vphys_data.find("[", tri_idx);
      size_t close_bracket = vphys_data.find("]", open_bracket);
      if (open_bracket != std::string::npos &&
          close_bracket != std::string::npos) {
        std::string block =
            vphys_data.substr(open_bracket, close_bracket - open_bracket);
        size_t p = 0;
        while ((p = block.find("[", p)) != std::string::npos) {
          int i = 0, j = 0, k = 0;
          if (sscanf(block.c_str() + p, "[ %d, %d, %d ]", &i, &j, &k) == 3) {
            parsed_indices.push_back(i);
            parsed_indices.push_back(j);
            parsed_indices.push_back(k);
          }
          p++;
        }
      }
    }

    if (!parsed_positions.empty() && !parsed_indices.empty()) {
      for (size_t i = 0; i + 2 < parsed_indices.size(); i += 3) {
        int i0 = parsed_indices[i];
        int i1 = parsed_indices[i + 1];
        int i2 = parsed_indices[i + 2];

        if (i0 >= 0 && i0 < parsed_positions.size() && i1 >= 0 &&
            i1 < parsed_positions.size() && i2 >= 0 &&
            i2 < parsed_positions.size()) {
          Triangle tri;
          tri.v0 = parsed_positions[i0];
          tri.v1 = parsed_positions[i1];
          tri.v2 = parsed_positions[i2];
          tri.min_x = std::min({tri.v0.x, tri.v1.x, tri.v2.x});
          tri.max_x = std::max({tri.v0.x, tri.v1.x, tri.v2.x});
          tri.min_y = std::min({tri.v0.y, tri.v1.y, tri.v2.y});
          tri.max_y = std::max({tri.v0.y, tri.v1.y, tri.v2.y});
          tri.min_z = std::min({tri.v0.z, tri.v1.z, tri.v2.z});
          tri.max_z = std::max({tri.v0.z, tri.v1.z, tri.v2.z});
          map_triangles.push_back(tri);
        }
      }
    }
  }

  if (!map_triangles.empty()) {
    is_map_loaded = true;
    return true;
  }

  return false;
}

bool IntersectRayTriangle(const Vector3 &orig, const Vector3 &dir, float &t,
                          const Triangle &tri) {
  float kEpsilon = 1e-6f;
  Vector3 edge1 = {tri.v1.x - tri.v0.x, tri.v1.y - tri.v0.y,
                   tri.v1.z - tri.v0.z};
  Vector3 edge2 = {tri.v2.x - tri.v0.x, tri.v2.y - tri.v0.y,
                   tri.v2.z - tri.v0.z};

  Vector3 pvec = {dir.y * edge2.z - dir.z * edge2.y,
                  dir.z * edge2.x - dir.x * edge2.z,
                  dir.x * edge2.y - dir.y * edge2.x};

  float det = edge1.x * pvec.x + edge1.y * pvec.y + edge1.z * pvec.z;

  if (std::abs(det) < kEpsilon)
    return false;

  float invDet = 1.0f / det;
  Vector3 tvec = {orig.x - tri.v0.x, orig.y - tri.v0.y, orig.z - tri.v0.z};
  float u = (tvec.x * pvec.x + tvec.y * pvec.y + tvec.z * pvec.z) * invDet;

  if (u < 0.0f || u > 1.0f)
    return false;

  Vector3 qvec = {tvec.y * edge1.z - tvec.z * edge1.y,
                  tvec.z * edge1.x - tvec.x * edge1.z,
                  tvec.x * edge1.y - tvec.y * edge1.x};

  float v = (dir.x * qvec.x + dir.y * qvec.y + dir.z * qvec.z) * invDet;

  if (v < 0.0f || u + v > 1.0f)
    return false;

  t = (edge2.x * qvec.x + edge2.y * qvec.y + edge2.z * qvec.z) * invDet;
  return t > kEpsilon;
}

bool IsVisible(const Vector3 &start, const Vector3 &end) {
  if (!is_map_loaded)
    return true; // Default to visible if no map

  Vector3 dir = {end.x - start.x, end.y - start.y, end.z - start.z};
  float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (length == 0.0f)
    return true;

  dir.x /= length;
  dir.y /= length;
  dir.z /= length;

  float ray_min_x = std::min(start.x, end.x) - 1.0f;
  float ray_max_x = std::max(start.x, end.x) + 1.0f;
  float ray_min_y = std::min(start.y, end.y) - 1.0f;
  float ray_max_y = std::max(start.y, end.y) + 1.0f;
  float ray_min_z = std::min(start.z, end.z) - 1.0f;
  float ray_max_z = std::max(start.z, end.z) + 1.0f;

  for (const auto &tri : map_triangles) {
    if (tri.max_x < ray_min_x || tri.min_x > ray_max_x ||
        tri.max_y < ray_min_y || tri.min_y > ray_max_y ||
        tri.max_z < ray_min_z || tri.min_z > ray_max_z) {
      continue;
    }

    float t = 0.0f;
    if (IntersectRayTriangle(start, dir, t, tri)) {
      if (t > 0 && t < length)
        return false; // Collision
    }
  }
  return true;
}

float GetPenetrationDepth(const Vector3 &start, const Vector3 &end) {
  if (!is_map_loaded)
    return 0.0f;

  Vector3 dir = {end.x - start.x, end.y - start.y, end.z - start.z};
  float total_length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (total_length == 0.0f)
    return 0.0f;

  dir.x /= total_length;
  dir.y /= total_length;
  dir.z /= total_length;

  struct Hit {
    float t;
    bool is_entry; // CS2 uses simple logic here, we'll just sort hits
  };
  std::vector<float> hits;

  for (const auto &tri : map_triangles) {
    float t = 0.0f;
    if (IntersectRayTriangle(start, dir, t, tri)) {
      if (t > 0 && t < total_length) {
        hits.push_back(t);
      }
    }
  }

  if (hits.size() < 2)
    return 0.0f;

  std::sort(hits.begin(), hits.end());

  float depth = 0.0f;
  // Basic logic: depth = distance between first and last hit in a cluster
  // This is a simplification for wallbang, assuming one wall.
  depth = hits.back() - hits.front();

  return depth;
}
} // namespace BspParser
