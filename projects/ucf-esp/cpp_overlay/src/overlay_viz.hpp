#pragma once
#include <cstdint>
#include "shared_state.hpp"
#include "smooth.hpp"

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
    bool   visible = true;   // 遮挡检测结果：false = 被墙体阻挡（frida Linecast 写入）
    struct BoneScreen {
        float sx = 0, sy = 0;
        bool valid = false;
    } bones[MAX_BONES]{};
};

// 帧坐标 -> 屏幕物理像素 的映射（与 overlay_tk.compute_viewport 同义）。
struct Viewport {
    float x = 0, y = 0, w = 0, h = 0;  // 显示区矩形（屏幕物理像素）
    float sx = 1, sy = 1;              // 帧坐标 -> 像素 的缩放
};

enum class FitMode { Auto, Stretch, Letterbox };

// 根据真实游戏客户区计算帧坐标到桌面像素的映射。
// Auto 在宽高比接近时铺满，否则等比居中，避免 800x600 帧被拉成 16:9。
Viewport compute_viewport(float client_x, float client_y, float client_w, float client_h,
                          float frame_w, float frame_h, FitMode mode = FitMode::Auto);

struct BoxPrim   { float x, y, w, h; float r, g, b; float thickness = 2.0f; bool selected = false; bool blocked = false; };
struct BarPrim   { float x, y, w, h; float ratio; float r, g, b; };
struct LabelPrim { float x, y; float r, g, b; char text[64]; };
struct BoneLinePrim {
    float x1, y1, x2, y2; float r, g, b; float thickness = 1.5f;
    bool selected = false;
};
struct RayPrim { float x1, y1, x2, y2; float r, g, b; float thickness = 1.5f; };

struct DrawList {
    BoxPrim   boxes[64];   int boxCount = 0;
    BarPrim   bars[64];    int barCount = 0;
    LabelPrim labels[64];  int labelCount = 0;
    BoneLinePrim boneLines[MAX_BONES * 64]; int boneLineCount = 0;
    RayPrim rays[1];        int rayCount = 0;
};

struct DrawStyle {
    bool show_box = true;
    bool show_skeleton = false;
    bool show_health = true;
    bool show_distance = true;
    bool show_local = false;
    bool show_teammate = true;
    bool show_enemy = true;
    int target_index = -1;
    TargetState target_state = TargetState::Normal;
    bool show_target_ray = false;
    bool ray_from_bottom = true;
    float max_distance = 250.0f;
    float line_thickness = 2.0f;
    float local[3] = {0.22f, 0.83f, 0.33f};
    float teammate[3] = {0.35f, 0.65f, 1.0f};
    float enemy[3] = {0.97f, 0.32f, 0.29f};
};

// 把屏幕标记映射到视口并生成绘制原语。
// 屏幕外 / 已死亡 / 被裁剪的点不画（裁剪判定在投影阶段完成，这里只看 on_screen）。
void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n, DrawList& out);
void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n,
                     const DrawStyle& style, DrawList& out);

} // namespace ucf
