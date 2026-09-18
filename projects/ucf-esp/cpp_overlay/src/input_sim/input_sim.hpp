#pragma once

#include "../smooth.hpp"

namespace ucf::input_sim {

constexpr int kXButton1 = 0x05;
constexpr int kXButton2 = 0x06;

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct Config {
    bool enabled = false;
    int aim_key = kXButton2;
    int fire_key = kXButton1;
    float align_tolerance_px = 4.0f;
    int jitter_px = 2;
    float smooth_factor = 0.35f;
    int max_step_px = 96;
};

class Sender {
public:
    virtual ~Sender() = default;
    virtual void move(int dx, int dy) = 0;
    virtual void button_down(int key) = 0;
    virtual void button_up(int key) = 0;
};

// Windows 使用 SendInput；非 Windows 构建使用空实现，供核心库和单测编译。
class LocalInputSim {
public:
    explicit LocalInputSim(Sender* sender = nullptr);
    void set_config(const Config& config);
    const Config& config() const { return cfg_; }

    // 按住状态的后续帧：只移动，不执行开火。
    void on_hold_trace(const ScreenPoint& target_screen);
    // 触发键（侧键5）按住：平滑移动；进入容差后保持按住左键自动开火。
    void on_flick(const ScreenPoint& target_screen);
    // 瞄准键按住只 trace；触发键按住做甩枪+自动开火。
    void update(bool aim_key_down, bool fire_key_down,
                const ScreenPoint& target_screen,
                const ScreenPoint& crosshair, TargetState state);
    void reset_lock();

private:
    void move_to(const ScreenPoint& target, const ScreenPoint& crosshair);
    bool aligned(const ScreenPoint& target, const ScreenPoint& crosshair) const;
    void release_fire();

    Config cfg_{};
    Sender* sender_ = nullptr;
    bool was_fire_down_ = false;
    bool firing_ = false;   // 左键是否正按住（自动开火期间保持）
    ScreenPoint crosshair_{};
};

} // namespace ucf::input_sim
