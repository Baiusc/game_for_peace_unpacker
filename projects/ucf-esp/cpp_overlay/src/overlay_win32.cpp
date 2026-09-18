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
    }
}

// ---- 渲染到 ImGui 背景绘制层 ----------------------------------------------
static ImU32 rgb(float r, float g, float b) {
    return IM_COL32(std::uint8_t(r*255), std::uint8_t(g*255), std::uint8_t(b*255), 255);
}

void render_draw_list(ImDrawList* dl, const DrawList& d) {
    for (int i = 0; i < d.boxCount; ++i) {
        const BoxPrim& b = d.boxes[i];
        dl->AddRect(ImVec2(b.x, b.y), ImVec2(b.x + b.w, b.y + b.h), rgb(b.r, b.g, b.b), 0, 0, b.thickness);
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
        dl->AddText(ImVec2(L.x, L.y), rgb(L.r, L.g, L.b), L.text);
    }
    for (int i = 0; i < d.skeletonCount; ++i) {
        const SkeletonPrim& s = d.skeletons[i];
        const ImU32 c = rgb(s.r, s.g, s.b);
        const float cx = s.x + s.w * 0.5f;
        const float head = s.y + s.h * 0.16f;
        const float shoulders = s.y + s.h * 0.30f;
        const float hips = s.y + s.h * 0.62f;
        const float feet = s.y + s.h * 0.98f;
        dl->AddCircle(ImVec2(cx, head), s.w * 0.10f, c, 12, s.thickness);
        dl->AddLine(ImVec2(cx, head + s.w * 0.10f), ImVec2(cx, hips), c, s.thickness);
        dl->AddLine(ImVec2(s.x + s.w * 0.18f, shoulders), ImVec2(s.x + s.w * 0.82f, shoulders), c, s.thickness);
        dl->AddLine(ImVec2(cx, hips), ImVec2(s.x + s.w * 0.25f, feet), c, s.thickness);
        dl->AddLine(ImVec2(cx, hips), ImVec2(s.x + s.w * 0.75f, feet), c, s.thickness);
    }
}

} // namespace ucf
#endif
