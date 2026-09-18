#include "overlay_viz.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ucf {

Viewport compute_viewport(float client_x, float client_y, float client_w, float client_h,
                          float frame_w, float frame_h, FitMode mode) {
    client_w = std::max(1.0f, client_w);
    client_h = std::max(1.0f, client_h);
    frame_w = std::max(1.0f, frame_w);
    frame_h = std::max(1.0f, frame_h);
    if (mode == FitMode::Auto) {
        const float frame_aspect = frame_w / frame_h;
        const float client_aspect = client_w / client_h;
        const float relative_error = std::fabs(client_aspect - frame_aspect) / frame_aspect;
        mode = relative_error <= 0.02f ? FitMode::Stretch : FitMode::Letterbox;
    }
    if (mode == FitMode::Letterbox) {
        const float scale = std::min(client_w / frame_w, client_h / frame_h);
        const float w = frame_w * scale;
        const float h = frame_h * scale;
        return {client_x + (client_w - w) * 0.5f,
                client_y + (client_h - h) * 0.5f,
                w, h, scale, scale};
    }
    return {client_x, client_y, client_w, client_h,
            client_w / frame_w, client_h / frame_h};
}

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

static void health_color(float ratio, float out[3]) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    if (ratio < 0.5f) {
        const float t = ratio * 2.0f;
        out[0] = 1.0f; out[1] = t; out[2] = 0.05f;
    } else {
        const float t = (ratio - 0.5f) * 2.0f;
        out[0] = 1.0f - t; out[1] = 1.0f; out[2] = 0.05f;
    }
}

static void dim_color(float color[3]) {
    const float luminance = color[0] * 0.299f + color[1] * 0.587f + color[2] * 0.114f;
    // 保留色相，只降低饱和度/亮度；不能压到灰黑色，否则所有未选中目标都不可辨认。
    for (int i = 0; i < 3; ++i) color[i] = luminance * 0.55f + color[i] * 0.35f;
}

void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n, DrawList& out) {
    build_draw_list(vp, marks, n, DrawStyle{}, out);
}

void build_draw_list(const Viewport& vp, const ScreenMark* marks, int n,
                     const DrawStyle& style, DrawList& out) {
    out.boxCount = out.barCount = out.labelCount = out.boneLineCount = out.rayCount = 0;

    const float BOX_W = 40.0f, BOX_H = 60.0f;
    float bw = BOX_W * vp.sx;  if (bw < 6)  bw = 6;
    float bh = BOX_H * vp.sy;  if (bh < 9)  bh = 9;
    float bar_h = 4.0f * vp.sy; if (bar_h < 2) bar_h = 2;

    for (int i = 0; i < n; ++i) {
        const ScreenMark& m = marks[i];
        if (!m.on_screen || m.is_dead) continue;   // 屏幕外 / 死亡不画
        if (style.max_distance <= 0.0f || m.dist > style.max_distance) continue;
        if ((m.kind == Kind::Local && !style.show_local) ||
            (m.kind == Kind::Teammate && !style.show_teammate) ||
            (m.kind == Kind::Enemy && !style.show_enemy)) continue;

        float px = vp.x + m.sx * vp.sx;
        float py = vp.y + m.sy * vp.sy;
        float col[3]; kind_color(m.kind, style, col);
        const bool selected = (i == style.target_index);
        if (!selected) dim_color(col);

        if (style.show_box && out.boxCount < 64) {
            BoxPrim& b = out.boxes[out.boxCount++];
            b.x = px - bw / 2; b.y = py - bh / 2; b.w = bw; b.h = bh;
            b.r = col[0]; b.g = col[1]; b.b = col[2];
            b.thickness = style.line_thickness;
            b.selected = selected;
        }
        if (style.show_health && out.barCount < 64) {
            BarPrim& bar = out.bars[out.barCount++];
            bar.x = px - bw / 2; bar.y = py - bh / 2 - bar_h * 2;
            bar.w = bw; bar.h = bar_h;
            float ratio = (m.max_hp > 0) ? float(m.hp) / float(m.max_hp) : 0.0f;
            bar.ratio = ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio);
            float hp_col[3]{};
            health_color(ratio, hp_col);
            bar.r = hp_col[0]; bar.g = hp_col[1]; bar.b = hp_col[2];
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
                line.selected = selected;
            }
        }
        if (selected && style.show_target_ray && out.rayCount < 1) {
            RayPrim& ray = out.rays[out.rayCount++];
            ray.x1 = vp.x + vp.w * 0.5f;
            ray.y1 = style.ray_from_bottom ? vp.y + vp.h : vp.y + vp.h * 0.5f;
            ray.x2 = px; ray.y2 = py;
            if (style.target_state != TargetState::Normal) {
                ray.r = 1.0f; ray.g = 0.85f; ray.b = 0.1f;
            } else {
                ray.r = col[0]; ray.g = col[1]; ray.b = col[2];
            }
        }
    }
}

} // namespace ucf
