#pragma once
#ifdef _WIN32
#include "config.hpp"
// ImGui 菜单：开关 / 颜色 / FOV / 平滑 / 目标选择；INSERT 切换显隐。
void draw_menu(ucf::Settings& s, bool& show_menu);
#endif
