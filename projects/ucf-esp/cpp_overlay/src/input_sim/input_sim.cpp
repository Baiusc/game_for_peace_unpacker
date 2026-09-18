#include "input_sim.hpp"

#include <cmath>
#include <cstdlib>

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
}

void LocalInputSim::move_to(const ScreenPoint& target, const ScreenPoint& crosshair) {
    const int dx = static_cast<int>(std::lround(target.x - crosshair.x));
    const int dy = static_cast<int>(std::lround(target.y - crosshair.y));
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
    move_to(target_screen, crosshair);
    if (aligned(target_screen, crosshair)) {
        sender_->button_down(cfg_.fire_key);
        sender_->button_up(cfg_.fire_key);
    }
}

void LocalInputSim::update(bool key_down, const ScreenPoint& target_screen,
                           const ScreenPoint& crosshair, TargetState state) {
    if (!cfg_.enabled) {
        reset_lock();
        return;
    }
    if (!key_down) {
        reset_lock();
        return;
    }
    if (state != TargetState::Normal) {
        was_down_ = true;
        return;
    }
    crosshair_ = crosshair;
    if (!was_down_) on_tap_move_and_fire(target_screen, crosshair);
    else on_hold_trace(target_screen);
    was_down_ = true;
}

void LocalInputSim::reset_lock() { was_down_ = false; }

} // namespace ucf::input_sim
