#pragma once
#include <cstddef>
#include "shared_state.hpp"

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
    int   team  = 0;
    bool  dead  = false;
    bool  visible = true;
    bool  valid = true;
};

// 当前契约没有 LOS/遮挡字段；此谓词只检查 PlayerState 是否具备可用的
// 基础位置数据。屏幕投影有效性仍由 overlay_win32 在生成 Candidate 时补充。
// TODO: Frame 增加 visible 字段后，在这里合并真实可见性状态。
bool is_target_visible(const PlayerState& player);

// 在候选敌人里，挑出“落在 fov_deg 视锥内”的，再按 mode 选一个：
//   mode==0 -> 距离最近；mode==1 -> 血量最低；mode==2 -> 准星最近。
// 返回选中的下标；无可用目标返回 -1。
// cam_forward 为相机前向量（不必归一化）。
int select_target(const Vec3& cam_forward, const Candidate* c, int n,
                  float fov_deg, int mode);

// 屏幕空间选靶：只考虑 FOV 圆内目标，返回距离屏幕中心最近的下标。
// 这是叠加层显示与用户视线一致的选靶规则，不注入鼠标或输入。
struct ScreenCandidate {
    float x = 0, y = 0;
    bool valid = true;
};

// 目标状态只描述本地 Frame 中已知的信息；当前契约没有遮挡字段，
// visible=false 只代表上游没有提供可用投影/可见性标记，不推断游戏内 LOS。
enum class TargetState { Normal, Blocked, Dead, Invalid };
TargetState target_state(const Candidate& candidate);

struct LockPreventState {
    int last_target = -1;
    bool active = false;
};

// 目标刚从候选集中掉出（死亡/无效/不可见）时只抑制下一次重锁定。
// 不执行任何输入、视角或进程操作；key_held 仅表示上层本地模拟状态机是否保持激活。
bool lock_prevent_should_skip(LockPreventState& state, const Candidate* c, int n,
                              int current_target, bool enabled, bool key_held);
int select_screen_target(const ScreenCandidate* c, int n,
                         float center_x, float center_y, float fov_radius);

Angles angles_from_direction(Vec3 dir);
float angle_distance(Angles a, Angles b);

} // namespace ucf
