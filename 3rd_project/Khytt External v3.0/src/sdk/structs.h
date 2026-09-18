#pragma once
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vector2 {
  float x, y;
};

struct Vector3 {
  float x, y, z;

  Vector3 operator+(const Vector3 &other) const {
    return {x + other.x, y + other.y, z + other.z};
  }
  Vector3 operator-(const Vector3 &other) const {
    return {x - other.x, y - other.y, z - other.z};
  }
  Vector3 operator*(float scalar) const {
    return {x * scalar, y * scalar, z * scalar};
  }

  bool IsZero() const { return x == 0 && y == 0 && z == 0; }
  Vector3 ToAngle() const {
    Vector3 angles;
    float hyp = sqrt(x * x + y * y);
    angles.x = -atan2(z, hyp) * (180.0f / (float)M_PI);
    angles.y = atan2(y, x) * (180.0f / (float)M_PI);
    angles.z = 0.0f;
    return angles;
  }
};

struct Vector4 {
  float x, y, z, w;
};

struct GlobalVars {
  float real_time;
  int frame_count;
  float absolute_frame_time;
  float absolute_frame_start_time_std_dev;
  float current_time;
  float frame_time;
  int max_clients;
  int tick_count;
  float interval_per_tick;
  float interpolation_amount;
  int sim_ticks_this_frame;
  int network_protocol;
  void *save_data;
  bool client;
  bool remote_client;
};

struct view_matrix_t {
  float *operator[](int index) { return matrix[index]; }

  float matrix[4][4];
};

// Helper for W2S
inline bool WorldToScreen(const Vector3 &pos, Vector2 &out,
                          const view_matrix_t &matrix, float screen_width,
                          float screen_height) {
  float _x = matrix.matrix[0][0] * pos.x + matrix.matrix[0][1] * pos.y +
             matrix.matrix[0][2] * pos.z + matrix.matrix[0][3];
  float _y = matrix.matrix[1][0] * pos.x + matrix.matrix[1][1] * pos.y +
             matrix.matrix[1][2] * pos.z + matrix.matrix[1][3];
  float w = matrix.matrix[2][0] * pos.x + matrix.matrix[2][1] * pos.y +
            matrix.matrix[2][2] * pos.z + matrix.matrix[2][3];

  if (w < 0.01f)
    return false;

  float inv_w = 1.f / w;
  _x *= inv_w;
  _y *= inv_w;

  float x = screen_width / 2.f;
  float y = screen_height / 2.f;

  x += 0.5f * _x * screen_width + 0.5f;
  y -= 0.5f * _y * screen_height + 0.5f;

  out.x = x;
  out.y = y;

  return true;
}

inline Vector3 CalculateAngle(const Vector3 &src, const Vector3 &dst) {
  Vector3 delta = dst - src;
  return delta.ToAngle();
}

inline float CalculateFov(const Vector3 &view_angles,
                          const Vector3 &target_angle) {
  Vector3 delta = target_angle - view_angles;
  // Normalize
  while (delta.y > 180)
    delta.y -= 360;
  while (delta.y < -180)
    delta.y += 360;
  return sqrt(delta.x * delta.x + delta.y * delta.y);
}
