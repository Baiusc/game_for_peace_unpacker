#include "overlay_viz.hpp"
#include <cstdio>

namespace ucf {

static void kind_color(Kind k, float out[3]) {
    switch (k) {
        case Kind::Local:     out[0] = 0.22f; out[1] = 0.83f; out[2] = 0.33f; break;
        case Kind::Teammate:  out[0] = 0.35f; out[1] = 0.65f; out[2] = 1.00f; break;
        case Kind::Enemy:     out[0] = 0.97f; out[1] = 0.32f; out[2] = 0.29f; break;
        default:              out[0] = out[1] = out[2] = 1.0f; break;
    }
}

void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n, DrawList& out) {
    out.boxCount = out.barCount = out.labelCount = 0;

    const float BOX_W = 40.0f, BOX_H = 60.0f;
    float bw = BOX_W * vp.sx;  if (bw < 6)  bw = 6;
    float bh = BOX_H * vp.sy;  if (bh < 9)  bh = 9;
    float bar_h = 4.0f * vp.sy; if (bar_h < 2) bar_h = 2;

    for (int i = 0; i < n; ++i) {
        const ScreenMark& m = marks[i];
        if (!m.on_screen || m.is_dead) continue;   // 屏幕外 / 死亡不画

        float px = vp.x + m.sx * vp.sx;
        float py = vp.y + m.sy * vp.sy;
        float col[3]; kind_color(m.kind, col);

        if (out.boxCount < 64) {
            BoxPrim& b = out.boxes[out.boxCount++];
            b.x = px - bw / 2; b.y = py - bh / 2; b.w = bw; b.h = bh;
            b.r = col[0]; b.g = col[1]; b.b = col[2];
        }
        if (out.barCount < 64) {
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
            int off = snprintf(L.text, sizeof(L.text), "%s t", kn);
            if (m.max_hp > 0 && off < (int)sizeof(L.text))
                off += snprintf(L.text + off, sizeof(L.text) - off, "%d/%d", m.hp, m.max_hp);
            if (m.dist > 0 && off < (int)sizeof(L.text))
                off += snprintf(L.text + off, sizeof(L.text) - off, " %.0fm", m.dist);
        }
    }
}

} // namespace ucf
