#include "overlay_win32.hpp"

#ifdef _WIN32
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
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

// ---------------------------------------------------------------------------
// 录制回放数据源 RecordedSource：从 JSONL 录制文件回放真实帧。
// 录制的写出格式见 main_win32.cpp 的 record_frame()（schema_version 2）。
// 这里用一个最小 JSON 解析器按已知 schema 反序列化，不引入第三方 JSON 库。
// ---------------------------------------------------------------------------
namespace {

struct JVal {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;
    const JVal* find(const char* k) const {
        if (type != OBJ) return nullptr;
        for (const auto& kv : obj) if (kv.first == k) return &kv.second;
        return nullptr;
    }
};

struct JsonParser {
    const char* p = nullptr;
    const char* end = nullptr;
    bool ok = true;

    static int hexval(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }
    void skip() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    JVal parse() { skip(); return parseValue(); }

    JVal parseValue() {
        skip();
        if (p >= end) { ok = false; return {}; }
        const char c = *p;
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') { JVal v; v.type = JVal::STR; v.str = parseString(); return v; }
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { p += 4; return {}; }
        return parseNumber();
    }
    JVal parseObject() {
        JVal v; v.type = JVal::OBJ; ++p;
        skip();
        if (p < end && *p == '}') { ++p; return v; }
        while (true) {
            skip();
            if (p >= end || *p != '"') { ok = false; return v; }
            std::string key = parseString();
            skip();
            if (p >= end || *p != ':') { ok = false; return v; }
            ++p;
            JVal val = parseValue();
            v.obj.emplace_back(std::move(key), std::move(val));
            skip();
            if (p >= end) { ok = false; return v; }
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; break; }
            ok = false; break;
        }
        return v;
    }
    JVal parseArray() {
        JVal v; v.type = JVal::ARR; ++p;
        skip();
        if (p < end && *p == ']') { ++p; return v; }
        while (true) {
            JVal val = parseValue();
            v.arr.push_back(std::move(val));
            skip();
            if (p >= end) { ok = false; return v; }
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; break; }
            ok = false; break;
        }
        return v;
    }
    JVal parseBool() {
        JVal v; v.type = JVal::BOOL;
        if (std::strncmp(p, "true", 4) == 0) { v.b = true; p += 4; }
        else if (std::strncmp(p, "false", 5) == 0) { v.b = false; p += 5; }
        else ok = false;
        return v;
    }
    JVal parseNumber() {
        JVal v; v.type = JVal::NUM;
        const char* start = p;
        while (p < end && (std::isdigit((unsigned char)*p) || *p == '-' || *p == '+' ||
                           *p == '.' || *p == 'e' || *p == 'E')) ++p;
        v.num = std::strtod(start, nullptr);
        return v;
    }
    std::string parseString() {
        ++p; // 跳过开头的 "
        std::string out;
        while (p < end) {
            const char c = *p;
            if (c == '"') { ++p; return out; }
            if (c == '\\') {
                ++p;
                if (p >= end) { ok = false; return out; }
                const char e = *p++;
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (p + 4 > end) { ok = false; return out; }
                        int cp = 0;
                        for (int i = 0; i < 4; ++i) cp = cp * 16 + hexval(*p++);
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: ok = false; return out;
                }
            } else {
                out += c; ++p;
            }
        }
        ok = false; return out;
    }
};

void read_floats(const JVal& v, float* dst, int n) {
    if (v.type != JVal::ARR) return;
    for (int i = 0; i < n && i < (int)v.arr.size(); ++i)
        if (v.arr[i].type == JVal::NUM) dst[i] = (float)v.arr[i].num;
}

void parse_player(const JVal& j, PlayerState& p) {
    if (j.type != JVal::OBJ) return;
    if (const JVal* a = j.find("pos"))    read_floats(*a, p.pos, 3);
    if (const JVal* a = j.find("team"))   if (a->type == JVal::NUM) p.team = (int)a->num;
    if (const JVal* a = j.find("hp"))     if (a->type == JVal::NUM) p.hp = (int)a->num;
    if (const JVal* a = j.find("maxHp"))  if (a->type == JVal::NUM) p.maxHp = (int)a->num;
    if (const JVal* a = j.find("isDead")) if (a->type == JVal::BOOL) p.isDead = a->b;
    if (const JVal* a = j.find("visible")) if (a->type == JVal::BOOL) p.visible = a->b;
    if (const JVal* a = j.find("name"))   if (a->type == JVal::STR) {
        std::strncpy(p.name, a->str.c_str(), sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = 0;
    }
    if (const JVal* a = j.find("bones")) {
        if (a->type == JVal::ARR) {
            for (int i = 0; i < MAX_BONES && i < (int)a->arr.size(); ++i) {
                const JVal& b = a->arr[i];
                if (b.type != JVal::OBJ) continue;
                if (const JVal* pos = b.find("pos")) read_floats(*pos, p.bones[i].pos, 3);
                if (const JVal* valid = b.find("valid")) if (valid->type == JVal::BOOL) p.bones[i].valid = valid->b;
            }
        }
    }
}

void parse_frame(const JVal& j, Frame& f) {
    if (j.type != JVal::OBJ) return;
    if (const JVal* a = j.find("w2c"))    read_floats(*a, f.w2c, 16);
    if (const JVal* a = j.find("proj"))   read_floats(*a, f.proj, 16);
    if (const JVal* a = j.find("width"))  if (a->type == JVal::NUM) f.width = (int)a->num;
    if (const JVal* a = j.find("height")) if (a->type == JVal::NUM) f.height = (int)a->num;
    if (const JVal* a = j.find("inGame")) if (a->type == JVal::BOOL) f.inGame = a->b;
    if (const JVal* a = j.find("local"))  parse_player(*a, f.local);
    if (const JVal* a = j.find("players")) {
        if (a->type == JVal::ARR) {
            int n = 0;
            for (const auto& pl : a->arr) {
                if (n >= MAX_PLAYERS) break;
                parse_player(pl, f.players[n]);
                ++n;
            }
            f.playerCount = n;
        }
    }
    if (const JVal* a = j.find("playerCount")) if (a->type == JVal::NUM) f.playerCount = (int)a->num;
}

} // namespace

size_t RecordedSource::load(const char* path, size_t cap) {
    frames_.clear();
    cursor_ = 0.0;
    FILE* f = std::fopen(path, "rb");
    if (!f) return 0;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string content;
    if (sz > 0) { content.resize((size_t)sz); std::fread(&content[0], 1, (size_t)sz, f); }
    std::fclose(f);
    if (content.empty()) return 0;

    std::vector<Frame> loaded;
    loaded.reserve(512);
    size_t start = 0;
    while (start < content.size()) {
        size_t nl = content.find('\n', start);
        if (nl == std::string::npos) nl = content.size();
        size_t s = start, e = nl;
        while (s < e && (content[s] == ' ' || content[s] == '\r' || content[s] == '\t')) ++s;
        while (e > s && (content[e - 1] == ' ' || content[e - 1] == '\r' || content[e - 1] == '\t')) --e;
        if (e > s) {
            JsonParser p{content.c_str() + s, content.c_str() + e};
            const JVal root = p.parse();
            if (p.ok && root.type == JVal::OBJ) {
                Frame fr{};
                parse_frame(root, fr);
                if (fr.width > 0 && fr.height > 0) loaded.push_back(fr);
            }
        }
        start = nl + 1;
    }
    if (loaded.empty()) return 0;
    if (loaded.size() > cap) loaded.erase(loaded.begin(), loaded.begin() + (loaded.size() - cap));
    frames_ = std::move(loaded);
    return frames_.size();
}

void RecordedSource::update(Frame& out, float speed) {
    if (frames_.empty()) { out = Frame{}; return; }
    if (speed < 0.01f) speed = 1.0f;
    const size_t idx = (size_t)cursor_ % frames_.size();
    out = frames_[idx];
    cursor_ += double(speed);
    if (cursor_ >= double(frames_.size())) cursor_ -= double(frames_.size());
}

} // namespace ucf
#endif
