#pragma once
#ifdef _WIN32
#include "imgui.h"          // ImDrawList（render_draw_list 用到）
#include "shared_state.hpp"
#include "overlay_viz.hpp"

namespace ucf {

// 复刻 esp_core.project_frame 的列主序约定，把共享内存里的 Frame
// 投影成屏幕标记（kind / on_screen / 是否被裁剪都在这里决定）。
void project_frame(const Frame& f, ScreenMark* marks, int& n);

// 合成数据源：不连游戏时，用一组绕相机旋转的虚拟玩家喂数据，
// 便于在 Windows 上单独验证渲染/菜单/共享内存读取这条链路。
class SyntheticSource {
public:
    void update(Frame& out);
private:
    float t_ = 0.0f;
};

// 把 2D 原语画到 ImGui 背景绘制层（屏幕空间，盖在所有内容之下）。
void render_draw_list(ImDrawList* dl, const DrawList& d);

} // namespace ucf
#endif
