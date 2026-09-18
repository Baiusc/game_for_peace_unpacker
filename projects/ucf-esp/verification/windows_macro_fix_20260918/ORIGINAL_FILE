#include "input_sim.hpp"

#include <cmath>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ucf::input_sim {
namespace {

class PlatformSender final : public Sender {
public:
    void move(int dx, int dy) override {
#ifdef _WIN32
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        SendInput(1, &input, sizeof(input));
#else
        (void)dx; (void)dy;
#endif
    }

    void button_down(int key) override {
#ifdef _WIN32
        INPUT input{};
        input.type = INPUT_MOUSE;
        (void)key;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(input));
#else
        (void)key;
#endif
    }

    void button_up(int key) override {
#ifdef _WIN32
        INPUT input{};
        input.type = INPUT_MOUSE;
        (void)key;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(input));
#else
        (void)key;
#endif
    }
};

PlatformSender g_platform_sender;

} // namespace

LocalInputSim::LocalInputSim(Sender* sender)
    : sender_(sender ? sender : &g_platform_sender) {}

void LocalInputSim::set_config(const Config& config) {
    cfg_ = config;
    if (cfg_.align_tolerance_px < 0.0f) cfg_.align_tolerance_px = 0.0f;
    if (cfg_.jitter_px < 0) cfg_.jitter_px = 0;
    cfg_.smooth_factor = std::max(0.05f, std::min(1.0f, cfg_.smooth_factor));
    if (cfg_.max_step_px < 1) cfg_.max_step_px = 1;
}

void LocalInputSim::move_to(const ScreenPoint& target, const ScreenPoint& crosshair) {
    const float ex = target.x - crosshair.x;
    const float ey = target.y - crosshair.y;
    const float distance = std::sqrt(ex * ex + ey * ey);
    if (distance <= cfg_.align_tolerance_px) return;
    const float step_len = std::min(distance * cfg_.smooth_factor,
                                    static_cast<float>(cfg_.max_step_px));
    const float ux = ex / distance;
    const float uy = ey / distance;
    float jx = 0.0f, jy = 0.0f;
    if (cfg_.jitter_px > 0) {
        const int span = cfg_.jitter_px * 2 + 1;
        jx = static_cast<float>((std::rand() % span) - cfg_.jitter_px) * 0.5f;
        jy = static_cast<float>((std::rand() % span) - cfg_.jitter_px) * 0.5f;
    }
    int dx = static_cast<int>(std::lround(ux * step_len + jx));
    int dy = static_cast<int>(std::lround(uy * step_len + jy));
    if (dx == 0 && std::fabs(ex) > cfg_.align_tolerance_px) dx = ex > 0 ? 1 : -1;
    if (dy == 0 && std::fabs(ey) > cfg_.align_tolerance_px) dy = ey > 0 ? 1 : -1;
    if (std::fabs(static_cast<float>(dx)) > std::fabs(ex) + 1.0f)
        dx = static_cast<int>(std::lround(ex));
    if (std::fabs(static_cast<float>(dy)) > std::fabs(ey) + 1.0f)
        dy = static_cast<int>(std::lround(ey));
    if (dx != 0 || dy != 0) sender_->move(dx, dy);
}

bool LocalInputSim::aligned(const ScreenPoint& target, const ScreenPoint& crosshair) const {
    const float dx = target.x - crosshair.x;
    const float dy = target.y - crosshair.y;
    const float jitter = cfg_.jitter_px > 0
        ? static_cast<float>(std::rand() % (cfg_.jitter_px + 1)) : 0.0f;
    const float tolerance = cfg_.align_tolerance_px + jitter;
    return dx * dx + dy * dy <= tolerance * tolerance;
}

void LocalInputSim::on_hold_trace(const ScreenPoint& target_screen) {
    if (!cfg_.enabled) return;
    // 按住分支只有移动路径，明确不触发 button_down/up。
    move_to(target_screen, crosshair_);
}

void LocalInputSim::on_tap_move_and_fire(const ScreenPoint& target_screen,
                                         const ScreenPoint& crosshair) {
    if (!cfg_.enabled) return;
    if (aligned(target_screen, crosshair)) {
        sender_->button_down(cfg_.fire_key);
        sender_->button_up(cfg_.fire_key);
        fire_pending_ = false;
    } else {
        move_to(target_screen, crosshair);
    }
}

void LocalInputSim::update(bool aim_key_down, bool fire_key_down,
                           const ScreenPoint& target_screen,
                           const ScreenPoint& crosshair, TargetState state) {
    if (!cfg_.enabled) {
        reset_lock();
        return;
    }
    if (state != TargetState::Normal) {
        // 阻挡/死亡/无效目标不产生输入；同步边沿状态，避免目标恢复时误触发。
        was_fire_down_ = fire_key_down;
        fire_pending_ = false;
        return;
    }
    crosshair_ = crosshair;
    if (aim_key_down) on_hold_trace(target_screen);
    if (fire_key_down && !was_fire_down_) fire_pending_ = true;
    if (fire_key_down && fire_pending_) on_tap_move_and_fire(target_screen, crosshair);
    if (!fire_key_down) fire_pending_ = false;
    was_fire_down_ = fire_key_down;
}

void LocalInputSim::reset_lock() {
    was_fire_down_ = false;
    fire_pending_ = false;
}

} // namespace ucf::input_sim
