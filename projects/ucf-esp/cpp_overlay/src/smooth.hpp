#pragma once
#include <cstddef>

// 相机角度平滑 + 目标选择（纯数学，不注入鼠标、不碰任何进程）。
//
// 重要边界（按你的要求）：
//   - 这里只计算“期望的相机角度”，并把结果写到你自己的游戏相机上；
//   - 不调用 SendInput / mouse_event / 任何驱动级输入，也不对第三方进程做任何事；
//   - 你拥有该单机游戏源码，最终把返回的角度写给自己的 Camera 组件即可。
namespace ucf {

struct Angles { float yaw = 0, pitch = 0; };   // 弧度
struct Vec3   { float x = 0, y = 0, z = 0; };

// 指数平滑：current 朝 target 逼近，responsiveness∈(0,1]，越大越“跟手”。
// 每帧调用一次（dt 仅用于说明；本实现按帧率无关的系数逼近）。
Angles smooth_angles(Angles current, Angles target, float responsiveness);

// 目标选择候选：敌人相对相机的方向向量 + 距离 + 血量。
struct Candidate {
    Vec3  dir   = {};   // 敌人相对相机的位置向量（不必归一化）
    float dist  = 0;    // 距离（米），用于“最近”模式
    int   hp    = 0;    // 血量，用于“最低血量”模式
};

// 在候选敌人里，挑出“落在 fov_deg 视锥内”的，再按 mode 选一个：
//   mode==0 -> 距离最近；mode==1 -> 血量最低。
// 返回选中的下标；无可用目标返回 -1。
// cam_forward 为相机前向量（不必归一化）。
int select_target(const Vec3& cam_forward, const Candidate* c, int n,
                  float fov_deg, int mode);

} // namespace ucf
