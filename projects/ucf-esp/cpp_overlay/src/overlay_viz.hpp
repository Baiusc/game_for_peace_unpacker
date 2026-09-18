#pragma once
#include <cstdint>

// 通用 2D 可视化原语：把“投影后的屏幕标记”转成一组可直接交给
// ImGui 背景绘制层（GetBackgroundDrawList）去画的形状。
// 这一部分完全平台无关、可单元测试，不碰任何 D3D / Win32。
namespace ucf {

enum class Kind { Local, Teammate, Enemy };

struct ScreenMark {
    Kind   kind;
    float  sx = 0, sy = 0;   // 帧坐标系内的屏幕坐标（投影后，未做视口映射）
    int    hp = 100, max_hp = 100;
    float  dist = 0;         // 到相机距离（米）
    bool   on_screen = false;
    bool   is_dead = false;
};

// 帧坐标 -> 屏幕物理像素 的映射（与 overlay_tk.compute_viewport 同义）。
struct Viewport {
    float x = 0, y = 0, w = 0, h = 0;  // 显示区矩形（屏幕物理像素）
    float sx = 1, sy = 1;              // 帧坐标 -> 像素 的缩放
};

struct BoxPrim   { float x, y, w, h; float r, g, b; };
struct BarPrim   { float x, y, w, h; float ratio; float r, g, b; };
struct LabelPrim { float x, y; float r, g, b; char text[64]; };

struct DrawList {
    BoxPrim   boxes[64];   int boxCount = 0;
    BarPrim   bars[64];    int barCount = 0;
    LabelPrim labels[64];  int labelCount = 0;
};

// 把屏幕标记映射到视口并生成绘制原语。
// 屏幕外 / 已死亡 / 被裁剪的点不画（裁剪判定在投影阶段完成，这里只看 on_screen）。
void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n, DrawList& out);

} // namespace ucf
