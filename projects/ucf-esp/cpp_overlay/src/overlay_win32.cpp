#include "overlay_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <array>

namespace ucf {

// ---- 投影（列主序扁平 16，与 src/projection.cpp 完全一致）----------------
static std::array<float,16> combine_pv(const float P[16], const float V[16]) {
    std::array<float,16> VP{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += P[k*4+r] * V[c*4+k];
            VP[c*4+r] = s;
        }
    return VP;
}

static bool clip_of(const float p[3], const std::array<float,16>& m,
                    float& ndcx, float& ndcy, float& wc) {
    wc = p[0]*m[3] + p[1]*m[7] + p[2]*m[11] + m[15];
    if (wc <= 1e-3f) return false;          // 点在相机后方
    ndcx = (p[0]*m[0] + p[1]*m[4] + p[2]*m[8]  + m[12]) / wc;
    ndcy = (p[0]*m[1] + p[1]*m[5] + p[2]*m[9]  + m[13]) / wc;
    return true;
}

void project_frame(const Frame& f, ScreenMark* marks, int& n) {
    constexpr float NDC_CLIP = 3.0f;
    auto VP = combine_pv(f.proj, f.w2c);
    n = 0;
    const PlayerState* src[1 + MAX_PLAYERS];
    bool isLocal[1 + MAX_PLAYERS];
    src[0] = &f.local; isLocal[0] = true;
    for (int i = 0; i < f.playerCount && i < MAX_PLAYERS; ++i) {
        src[1+i] = &f.players[i]; isLocal[1+i] = false;
    }
    int total = 1 + (f.playerCount < MAX_PLAYERS ? f.playerCount : MAX_PLAYERS);

    for (int i = 0; i < total; ++i) {
        const PlayerState& p = *src[i];
        ScreenMark& m = marks[n];
        float ndcx, ndcy, wc;
        bool ok = clip_of(p.pos, VP, ndcx, ndcy, wc);
        float sx = (ndcx * 0.5f + 0.5f) * f.width;
        float sy = (1.0f - (ndcy * 0.5f + 0.5f)) * f.height;
        bool clipped = !ok || std::fabs(ndcx) > NDC_CLIP || std::fabs(ndcy) > NDC_CLIP;
        m.sx = sx; m.sy = sy;
        m.hp = p.hp; m.max_hp = p.maxHp;
        m.is_dead = p.isDead || p.hp <= 0;
        m.visible = p.visible;
        m.on_screen = f.inGame && (!clipped && sx >= 0 && sx <= f.width && sy >= 0 && sy <= f.height);
        if (isLocal[i]) {
            m.kind = Kind::Local;
            m.dist = 0.0f;
        } else {
            m.kind = (p.team == f.local.team) ? Kind::Teammate : Kind::Enemy;
            const float dx = p.pos[0] - f.local.pos[0];
            const float dy = p.pos[1] - f.local.pos[1];
            const float dz = p.pos[2] - f.local.pos[2];
            m.dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        for (int bi = 0; bi < MAX_BONES; ++bi) {
            const BoneState& bone = p.bones[bi];
            auto& bs = m.bones[bi];
            bs.valid = false;
            if (!bone.valid) continue;
            float bndcx = 0, bndcy = 0, bwc = 0;
            if (!clip_of(bone.pos, VP, bndcx, bndcy, bwc)) continue;
            const float bx = (bndcx * 0.5f + 0.5f) * f.width;
            const float by = (1.0f - (bndcy * 0.5f + 0.5f)) * f.height;
            if (std::fabs(bndcx) > NDC_CLIP || std::fabs(bndcy) > NDC_CLIP ||
                bx < 0 || bx > f.width || by < 0 || by > f.height) continue;
            bs.sx = bx; bs.sy = by; bs.valid = true;
        }
        ++n;
    }
}

// ---- 合成数据源 ------------------------------------------------------------
void SyntheticSource::update(Frame& out) {
    t_ += 0.016f;
    out.width = 1280; out.height = 720; out.inGame = true; out.playerCount = 12;

    // 相机在 (0,0,6) 看向 -Z；w2c = 平移 (0,0,-6)
    std::memset(out.w2c, 0, sizeof(out.w2c));
    out.w2c[0] = out.w2c[5] = out.w2c[10] = out.w2c[15] = 1.0f;
    out.w2c[14] = -6.0f;

    // 透视投影（列主序），fovy=45°, aspect=16:9
    float aspect = float(out.width) / float(out.height);
    float f = 1.0f / std::tan(0.785398f / 2.0f);   // ~2.414
    float nearp = 0.1f, farp = 100.0f;
    std::memset(out.proj, 0, sizeof(out.proj));
    out.proj[0]  = f / aspect;
    out.proj[5]  = f;
    out.proj[10] = (farp + nearp) / (nearp - farp);
    out.proj[11] = -1.0f;
    out.proj[14] = (2.0f * farp * nearp) / (nearp - farp);

    for (int i = 0; i < out.playerCount; ++i) {
        float a = t_ * (0.6f + 0.05f * i) + i * 0.7f;
        float rad = 2.0f + 1.5f * std::sin(t_ * 0.3f + i);
        out.players[i].pos[0] = std::cos(a) * rad;
        out.players[i].pos[1] = std::sin(a * 0.7f) * 1.5f;
        out.players[i].pos[2] = std::sin(a) * rad;
        out.players[i].hp = 30 + int(70 * (0.5f + 0.5f * std::sin(t_ + i)));
        out.players[i].maxHp = 100;
        out.players[i].team = i % 2;
        out.players[i].isDead = (out.players[i].hp <= 0);
        // 合成源也填充真实契约的 19 个骨骼槽位，便于离线验证连线渲染。
        static const float bone_offsets[MAX_BONES][3] = {
            {0, .90f, 0}, {-.15f, .60f, 0}, {.15f, .60f, 0},
            {-.15f, .30f, 0}, {.15f, .30f, 0}, {-.15f, 0, 0}, {.15f, 0, 0},
            {0, 1.10f, 0}, {0, 1.35f, 0}, {0, 1.55f, 0}, {0, 1.80f, 0},
            {-.25f, 1.40f, 0}, {.25f, 1.40f, 0}, {-.45f, 1.25f, 0},
            {.45f, 1.25f, 0}, {-.60f, 1.10f, 0}, {.60f, 1.10f, 0},
            {-.75f, 1.00f, 0}, {.75f, 1.00f, 0},
        };
        for (int bi = 0; bi < MAX_BONES; ++bi) {
            out.players[i].bones[bi].pos[0] = out.players[i].pos[0] + bone_offsets[bi][0];
            out.players[i].bones[bi].pos[1] = out.players[i].pos[1] + bone_offsets[bi][1];
            out.players[i].bones[bi].pos[2] = out.players[i].pos[2] + bone_offsets[bi][2];
            out.players[i].bones[bi].valid = true;
        }
    }
}

// ---- 渲染到 ImGui 背景绘制层 ----------------------------------------------
static ImU32 rgb(float r, float g, float b) {
    return IM_COL32(std::uint8_t(r*255), std::uint8_t(g*255), std::uint8_t(b*255), 255);
}

static void add_corner_box(ImDrawList* dl, float x, float y, float w, float h,
                           ImU32 color, float thickness) {
    const float cx = w * 0.25f, cy = h * 0.25f;
    dl->AddLine({x, y}, {x + cx, y}, color, thickness);
    dl->AddLine({x, y}, {x, y + cy}, color, thickness);
    dl->AddLine({x + w, y}, {x + w - cx, y}, color, thickness);
    dl->AddLine({x + w, y}, {x + w, y + cy}, color, thickness);
    dl->AddLine({x, y + h}, {x + cx, y + h}, color, thickness);
    dl->AddLine({x, y + h}, {x, y + h - cy}, color, thickness);
    dl->AddLine({x + w, y + h}, {x + w - cx, y + h}, color, thickness);
    dl->AddLine({x + w, y + h}, {x + w, y + h - cy}, color, thickness);
}

static void add_dashed_line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color,
                            float thickness, float dash = 6.0f) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) return;
    const float ux = dx / length, uy = dy / length;
    for (float t = 0.0f; t < length; t += dash * 2.0f) {
        const float end = std::min(t + dash, length);
        dl->AddLine({a.x + ux * t, a.y + uy * t},
                    {a.x + ux * end, a.y + uy * end}, color, thickness);
    }
}

void render_draw_list(ImDrawList* dl, const DrawList& d) {
    for (int i = 0; i < d.boxCount; ++i) {
        const BoxPrim& b = d.boxes[i];
        const ImU32 color = rgb(b.r, b.g, b.b);
        if (b.blocked) {
            // 被墙体遮挡：橙色虚线框，明确区别于“可见实线框”
            const ImU32 c = IM_COL32(255, 217, 26, 255);
            const float t = b.thickness;
            add_dashed_line(dl, {b.x, b.y}, {b.x + b.w, b.y}, c, t);
            add_dashed_line(dl, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, c, t);
            add_dashed_line(dl, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}, c, t);
            add_dashed_line(dl, {b.x, b.y + b.h}, {b.x, b.y}, c, t);
        } else if (b.selected) {
            dl->AddRectFilled(ImVec2(b.x, b.y), ImVec2(b.x + b.w, b.y + b.h), IM_COL32(0, 0, 0, 24));
            dl->AddRect(ImVec2(b.x, b.y), ImVec2(b.x + b.w, b.y + b.h), IM_COL32(0, 0, 0, 230), 0, 0, b.thickness + 2.0f);
            dl->AddRect(ImVec2(b.x, b.y), ImVec2(b.x + b.w, b.y + b.h), color, 0, 0, b.thickness);
        } else {
            add_corner_box(dl, b.x, b.y, b.w, b.h, color, b.thickness);
        }
    }
    for (int i = 0; i < d.barCount; ++i) {
        const BarPrim& bar = d.bars[i];
        dl->AddRectFilled(ImVec2(bar.x, bar.y), ImVec2(bar.x + bar.w, bar.y + bar.h),
                          IM_COL32(20, 20, 20, 200));
        dl->AddRectFilled(ImVec2(bar.x, bar.y),
                          ImVec2(bar.x + bar.w * bar.ratio, bar.y + bar.h),
                          rgb(bar.r, bar.g, bar.b));
    }
    for (int i = 0; i < d.labelCount; ++i) {
        const LabelPrim& L = d.labels[i];
        dl->AddText(ImVec2(L.x + 1.0f, L.y + 1.0f), IM_COL32(0, 0, 0, 220), L.text);
        dl->AddText(ImVec2(L.x, L.y), rgb(L.r, L.g, L.b), L.text);
    }
    for (int i = 0; i < d.boneLineCount; ++i) {
        const BoneLinePrim& line = d.boneLines[i];
        const ImU32 color = rgb(line.r, line.g, line.b);
        if (line.selected) {
            dl->AddLine(ImVec2(line.x1, line.y1), ImVec2(line.x2, line.y2), color, line.thickness);
        } else {
            add_dashed_line(dl, {line.x1, line.y1}, {line.x2, line.y2}, color, line.thickness * 0.9f);
        }
    }
    for (int i = 0; i < d.rayCount; ++i) {
        const RayPrim& ray = d.rays[i];
        dl->AddLine({ray.x1, ray.y1}, {ray.x2, ray.y2}, rgb(ray.r, ray.g, ray.b), ray.thickness);
    }
}

} // namespace ucf
#endif
