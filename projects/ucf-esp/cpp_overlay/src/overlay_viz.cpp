#include "overlay_viz.hpp"
#include <algorithm>
#include <cstdio>

namespace ucf {

static constexpr int BONE_PAIRS[][2] = {
    {0, 7}, {7, 8}, {8, 9}, {9, 10},
    {8, 11}, {11, 13}, {13, 15}, {15, 17},
    {8, 12}, {12, 14}, {14, 16}, {16, 18},
    {0, 1}, {1, 3}, {3, 5},
    {0, 2}, {2, 4}, {4, 6},
};
static constexpr int BONE_PAIR_COUNT = sizeof(BONE_PAIRS) / sizeof(BONE_PAIRS[0]);

static void kind_color(Kind k, const DrawStyle& style, float out[3]) {
    switch (k) {
        case Kind::Local:     std::copy(style.local, style.local + 3, out); break;
        case Kind::Teammate:  std::copy(style.teammate, style.teammate + 3, out); break;
        case Kind::Enemy:     std::copy(style.enemy, style.enemy + 3, out); break;
        default:              out[0] = out[1] = out[2] = 1.0f; break;
    }
}

void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n, DrawList& out) {
    build_draw_list(vp, marks, n, DrawStyle{}, out);
}

void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n,
                     const DrawStyle& style, DrawList& out) {
    out.boxCount = out.barCount = out.labelCount = out.boneLineCount = 0;

    const float BOX_W = 40.0f, BOX_H = 60.0f;
    float bw = BOX_W * vp.sx;  if (bw < 6)  bw = 6;
    float bh = BOX_H * vp.sy;  if (bh < 9)  bh = 9;
    float bar_h = 4.0f * vp.sy; if (bar_h < 2) bar_h = 2;

    for (int i = 0; i < n; ++i) {
        const ScreenMark& m = marks[i];
        if (!m.on_screen || m.is_dead) continue;   // 屏幕外 / 死亡不画
        if (style.max_distance > 0.0f && m.dist > style.max_distance) continue;

        float px = vp.x + m.sx * vp.sx;
        float py = vp.y + m.sy * vp.sy;
        float col[3]; kind_color(m.kind, style, col);

        if (style.show_box && out.boxCount < 64) {
            BoxPrim& b = out.boxes[out.boxCount++];
            b.x = px - bw / 2; b.y = py - bh / 2; b.w = bw; b.h = bh;
            b.r = col[0]; b.g = col[1]; b.b = col[2];
            b.thickness = style.line_thickness;
        }
        if (style.show_health && out.barCount < 64) {
            BarPrim& bar = out.bars[out.barCount++];
            bar.x = px - bw / 2; bar.y = py - bh / 2 - bar_h * 2;
            bar.w = bw; bar.h = bar_h;
            float ratio = (m.max_hp > 0) ? float(m.hp) / float(m.max_hp) : 0.0f;
            bar.ratio = ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio);
            bar.r = col[0]; bar.g = col[1]; bar.b = col[2];
        }
        if (out.labelCount < 64) {
            LabelPrim& L = out.labels[out.labelCount++];
            L.x = px; L.y = py - bh / 2 - bar_h * 2.6f;
            L.r = col[0]; L.g = col[1]; L.b = col[2];
            const char* kn = (m.kind == Kind::Local) ? "local"
                           : (m.kind == Kind::Teammate) ? "teammate" : "enemy";
            int off = snprintf(L.text, sizeof(L.text), "%s ", kn);
            if (m.max_hp > 0 && style.show_health && off < (int)sizeof(L.text))
                off += snprintf(L.text + off, sizeof(L.text) - off, "%d/%d", m.hp, m.max_hp);
            if (m.dist > 0 && style.show_distance && off < (int)sizeof(L.text))
                off += snprintf(L.text + off, sizeof(L.text) - off, " %.0fm", m.dist);
        }
        if (style.show_skeleton) {
            for (int pair = 0; pair < BONE_PAIR_COUNT && out.boneLineCount < MAX_BONES * 64; ++pair) {
                const auto& a = m.bones[BONE_PAIRS[pair][0]];
                const auto& b = m.bones[BONE_PAIRS[pair][1]];
                if (!a.valid || !b.valid) continue;
                BoneLinePrim& line = out.boneLines[out.boneLineCount++];
                line.x1 = vp.x + a.sx * vp.sx; line.y1 = vp.y + a.sy * vp.sy;
                line.x2 = vp.x + b.sx * vp.sx; line.y2 = vp.y + b.sy * vp.sy;
                line.r = col[0]; line.g = col[1]; line.b = col[2];
                line.thickness = style.line_thickness * 0.75f;
            }
        }
    }
}

} // namespace ucf
