#pragma once
#ifdef _WIN32
#include "imgui.h"          // ImDrawList（render_draw_list 用到）
#include "shared_state.hpp"
#include "overlay_viz.hpp"
#include <vector>           // RecordedSource::frames_

namespace ucf {

// 复刻 esp_core.project_frame 的列主序约定，把共享内存里的 Frame
// 投影成屏幕标记（kind / on_screen / 是否被裁剪都在这里决定）。
void project_frame(const Frame& f, ScreenMark* marks, int& n);

// 合成数据源：不连游戏、且没有可用录制文件时，用一组绕相机旋转的虚拟玩家喂数据，
// 便于在 Windows 上单独验证渲染/菜单/共享内存读取这条链路。
// （已有录制文件时优先走 RecordedSource 回放真实数据，synth 仅作最后兜底。）
class SyntheticSource {
public:
    void update(Frame& out);
private:
    float t_ = 0.0f;
};

// 录制回放数据源：不连游戏时，从录制文件（JSONL，schema_version 2，
// 与 main_win32.cpp 的 record_frame 写出格式一致）逐帧回放真实数据，
// 替代 SyntheticSource 的 13 个假玩家。按 speed 推进并循环播放。
class RecordedSource {
public:
    // 从 JSONL 录制文件加载（每行一个 frame）。cap 限制最多保留尾部 cap 帧（防内存膨胀）。
    // 返回成功加载的帧数（0 = 文件不存在/为空/解析失败）。
    size_t load(const char* path, size_t cap = 1000);
    bool   available() const { return !frames_.empty(); }
    size_t size() const { return frames_.size(); }
    // 取下一帧（cursor 按 speed 推进，到尾循环回开头）。speed<=0 视为 1。
    void   update(Frame& out, float speed = 1.0f);
    const Frame* at(size_t i) const { return i < frames_.size() ? &frames_[i] : nullptr; }
private:
    std::vector<Frame> frames_{};
    double             cursor_ = 0.0;
};

// 把 2D 原语画到 ImGui 背景绘制层（屏幕空间，盖在所有内容之下）。
void render_draw_list(ImDrawList* dl, const DrawList& d);

} // namespace ucf
#endif
