#include "smooth.hpp"
#include <cmath>

namespace ucf {

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

Angles smooth_angles(Angles current, Angles target, float responsiveness) {
    responsiveness = clampf(responsiveness, 0.0f, 1.0f);
    return Angles{
        current.yaw   + (target.yaw   - current.yaw)   * responsiveness,
        current.pitch + (target.pitch - current.pitch) * responsiveness,
    };
}

static Vec3 normalize(const Vec3& v) {
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l < 1e-6f) l = 1.0f;
    return {v.x / l, v.y / l, v.z / l};
}

static float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

int select_target(const Vec3& cam_forward, const Candidate* c, int n,
                  float fov_deg, int mode) {
    Vec3 f = normalize(cam_forward);
    float half = fov_deg * 0.5f * 3.14159265f / 180.0f;
    int best = -1;
    float bestMetric = (mode == 1) ? 1e30f : -1e30f;
    for (int i = 0; i < n; ++i) {
        Vec3 d = normalize(c[i].dir);
        float ang = std::acos(clampf(dot(f, d), -1.0f, 1.0f));
        if (ang > half) continue;                  // 不在视锥内
        if (mode == 0) {                           // 最近
            if (best < 0 || c[i].dist < bestMetric) {
                best = i; bestMetric = c[i].dist;
            }
        } else {                                   // 最低血量
            if (best < 0 || c[i].hp < bestMetric) {
                best = i; bestMetric = static_cast<float>(c[i].hp);
            }
        }
    }
    return best;
}

} // namespace ucf
