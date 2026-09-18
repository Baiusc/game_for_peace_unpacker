#pragma once
#ifdef _WIN32
#include "config.hpp"

namespace ucf {

// 菜单窗口矩形（客户区坐标），供主循环做点击穿透的命中判定。
struct MenuRect {
    bool  valid = false;
    float x = 0, y = 0, w = 0, h = 0;
};

// 主循环每帧回填、菜单内展示的诊断状态。
struct OverlayStatus {
    bool          flip = false;      // 交换链是否走 flip 模型
    bool          shm = false;       // 数据源：true=共享内存，false=合成
    int           players = 0;
    float         scale = 0.0f;
    unsigned long present_hr = 0;    // 上一帧 Present 的 HRESULT
};

MenuRect query_menu_rect();
void draw_menu(Settings& s, bool& show_menu, const OverlayStatus& st);
}
#endif
