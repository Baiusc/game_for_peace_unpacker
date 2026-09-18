#pragma once
#include <Windows.h>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include "imgui/imgui.h"
#include "../SDK/Game.h"
#include "../SDK/Structs.h"
#include "AvatarManager.h"

// Settings declared in Main.cpp
namespace Settings {
    extern bool esp_enabled, avatar_esp, health_bar, armor_bar, skeleton, names, distance,
                snaplines, head_dot, rainbow_global, box_2d, box_3d,
                fill_box, outline_box, rainbow_box, esp_enemies_only,
                fake_chams;
    extern int  box_style;
    extern float box_color[4];
    extern bool show_fov_circle;
    extern float fov_circle_color[4];
    extern bool hitsound;
    extern int  hitsound_sel;
    extern std::vector<std::string> hitsound_files;
    extern bool aim_enabled;
    extern bool aim_autowall;
    extern int  aim_key;
    extern int  aim_key_idx;
    extern bool grenade_prediction;
    extern bool fake_glow_enemies;
    extern bool watermark;
    extern bool spectator_list;
    extern float rgb_speed;
    extern float glow_spread;
    extern bool  hitmarker;
    extern float hitmarker_size;
    extern std::string exeDir;
}

// ────────────────────────────────────────────
//  Entity data snapshot (Double Buffered)
// ────────────────────────────────────────────
struct ESPData {
    bool active = false;
    Vector3 feet_pos;
    Vector3 head_pos;
    Vector3 origin;
    int health = 0;
    int team_id = 0;
    int armor = 0;
    char name[64] = { 0 };
    int ammo = 0;
    uintptr_t pawn = 0;
    uintptr_t controller = 0;
    float distance = 0.0f;
    uint64_t steam_id = 0;

    struct Bone {
        Vector3 pos;
        bool visible;
    };
    Bone bones[32]; 
    
    // Spectator info
    bool is_spectating_local = false;
};

struct HitMarker {
    Vector3 pos;
    int damage;
    float time;
    float alpha;
};

inline std::vector<HitMarker> g_HitMarkers;
inline std::mutex g_hit_mutex;

static std::vector<ESPData> g_front_buffer(64);
static std::vector<ESPData> g_back_buffer(64);
static std::mutex           g_data_mutex;
extern std::atomic<bool>    g_Running;

static int s_LastHP[65] = { 0 };

// ────────────────────────────────────────────
//  Math helpers
// ────────────────────────────────────────────
inline ImU32 GetRainbow(float speed = 1.f, float alpha = 1.f) {
    float t = (float)GetTickCount64() / 1000.f * speed * Settings::rgb_speed;
    float r = 0.5f + 0.5f * sinf(t);
    float g = 0.5f + 0.5f * sinf(t + 2.094f);
    float b = 0.5f + 0.5f * sinf(t + 4.188f);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(alpha * 255));
}

// CS2 Native Glow struct (CGlowProperty) - sub-offsets from dumper build 14138
// pawn + 0xCC0 = CGlowProperty base
// +0x08  m_fGlowColor        (Vector - float RGB)
// +0x30  m_iGlowType         (int)
// +0x34  m_iGlowTeam         (int, 0 = all)
// +0x40  m_glowColorOverride (int/Color32)
// +0x50  m_bEligibleForScreenHighlight (bool)
// +0x51  m_bGlowing          (bool)
static constexpr uintptr_t GLOW_OFFSET = 0xCC0; // m_Glow, build 14138

// Write glow color directly into the pawn's CGlowProperty
inline void SetNativeGlow(uintptr_t pawn, float r, float g, float b) {
    uintptr_t glowBase = pawn + GLOW_OFFSET;
    // Write RGB as a Vector3 into m_fGlowColor (+0x08)
    g_Memory->Write<float>(glowBase + 0x08, r);
    g_Memory->Write<float>(glowBase + 0x0C, g);
    g_Memory->Write<float>(glowBase + 0x10, b);
    // Set glow type (1 = world only glow, fully lit/solid effect)
    g_Memory->Write<int32_t>(glowBase + 0x30, 1);
    // Set team 0 = all teams visible
    g_Memory->Write<int32_t>(glowBase + 0x34, 0);
    // Enable glow
    g_Memory->Write<bool>(glowBase + 0x50, true);  // m_bEligibleForScreenHighlight
    g_Memory->Write<bool>(glowBase + 0x51, true);  // m_bGlowing
}

inline void DisableNativeGlow(uintptr_t pawn) {
    uintptr_t glowBase = pawn + GLOW_OFFSET;
    g_Memory->Write<bool>(glowBase + 0x50, false);
    g_Memory->Write<bool>(glowBase + 0x51, false);
}

inline void GetRainbowFloat(float speed, float& r, float& g, float& b) {
    float t = (float)GetTickCount64() / 1000.f * speed * Settings::rgb_speed;
    r = 0.5f + 0.5f * sinf(t);
    g = 0.5f + 0.5f * sinf(t + 2.094f);
    b = 0.5f + 0.5f * sinf(t + 4.188f);
}

// ────────────────────────────────────────────
//  Entity Reader Thread (Logic Ported from Khytt)
// ────────────────────────────────────────────
inline void EntityReaderThread() {
    while (g_Running) {
        if (!Settings::esp_enabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uintptr_t local_pawn = Game::GetLocalPlayerPawn();
        if (!local_pawn) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Get local player pawn handle for spectator comparison
        uintptr_t localController = Game::GetLocalPlayerController();
        uint32_t local_pawn_handle = localController ? g_Memory->Read<uint32_t>(localController + g_Off.m_hPlayerPawn) : 0;

        int local_team = g_Memory->Read<int>(local_pawn + g_Off.m_iTeamNum);
        uintptr_t ent_list = Game::GetEntityList();

        for (int i = 1; i <= 64; i++) {
            ESPData& data = g_back_buffer[i - 1];
            data.active = false;

            uintptr_t controller = Game::GetPlayerController(ent_list, i);
            if (!controller) continue;

            uintptr_t pawn_ent = Game::GetPawnFromController(controller, ent_list);
            if (!pawn_ent || pawn_ent == local_pawn) continue;

            uint32_t pawn_handle = g_Memory->Read<uint32_t>(controller + g_Off.m_hPlayerPawn);
            int pawn_index = pawn_handle & 0x7FFF;

            int health = g_Memory->Read<int>(pawn_ent + g_Off.m_iHealth);
            if (health <= 0 || health > 100) continue;

            int team = g_Memory->Read<int>(pawn_ent + g_Off.m_iTeamNum);
            if (Settings::esp_enemies_only && team == local_team) continue;
            
            data.active = true;
            data.health = health;
            data.team_id = team;
            data.pawn = pawn_ent;
            data.controller = controller;
            data.armor = g_Memory->Read<int>(pawn_ent + g_Off.m_ArmorValue);
            data.steam_id = g_Memory->Read<uint64_t>(controller + g_Off.m_steamID);

            uintptr_t scene_node = g_Memory->Read<uintptr_t>(pawn_ent + g_Off.m_pGameSceneNode);
            data.feet_pos = g_Memory->Read<Vector3>(scene_node + g_Off.m_nodeAbsOrigin);
            data.head_pos = data.feet_pos;
            data.head_pos.z += 72.0f;

            // --- Spectator Check ---
            data.is_spectating_local = false;
            uintptr_t obsSvc = g_Memory->Read<uintptr_t>(pawn_ent + g_Off.m_pObserverServices);
            if (obsSvc) {
                uint32_t obsTargetHandle = g_Memory->Read<uint32_t>(obsSvc + g_Off.m_hObserverTarget);
                if (obsTargetHandle != 0 && obsTargetHandle != 0xFFFFFFFF) {
                    uint32_t targetIndex = obsTargetHandle & 0x7FFF;
                    uint32_t localIndex = local_pawn_handle & 0x7FFF;
                    if (targetIndex == localIndex) {
                        data.is_spectating_local = true;
                    }
                }
            }

            // Distance calculation
            uintptr_t local_scene = g_Memory->Read<uintptr_t>(local_pawn + g_Off.m_pGameSceneNode);
            Vector3 local_pos = g_Memory->Read<Vector3>(local_scene + g_Off.m_nodeAbsOrigin);
            data.distance = (data.feet_pos - local_pos).Length() * 0.0254f; // Units to meters

            if (Settings::names) {
                for (int c = 0; c < 63; c++) {
                    data.name[c] = g_Memory->Read<char>(controller + g_Off.m_iszPlayerName + c);
                    if (!data.name[c]) break;
                }
            }

            if (Settings::skeleton || Settings::fake_glow_enemies) {
                uintptr_t bone_array = Game::GetBoneMatrix(pawn_ent);
                if (bone_array) {
                    const int MAX_BONE_INDEX = 30; 
                    Game::BoneEntry bone_buffer[MAX_BONE_INDEX];
                    if (g_Memory->ReadBytes(bone_array, bone_buffer, sizeof(bone_buffer))) {
                        int bones_to_read[] = {6, 5, 4, 13, 8, 14, 9, 15, 10, 25, 22, 26, 23, 27, 24};
                        for (int b : bones_to_read) {
                            if (b < MAX_BONE_INDEX) {
                                data.bones[b].pos = bone_buffer[b].pos;
                                data.bones[b].visible = !data.bones[b].pos.IsZero();
                            }
                        }
                    }
                }
            }

            // --- Hit & Damage Tracking ---
            int current_hp = data.health;
            if (s_LastHP[i] > 0 && current_hp < s_LastHP[i]) {
                int damage = s_LastHP[i] - current_hp;
                int crosshair_id = g_Memory->Read<int>(local_pawn + g_Off.m_iIDEntIndex);
                int target_index = crosshair_id & 0x7FFF;

                // Precision Check: Does crosshair point to this pawn?
                if (target_index == pawn_index && Settings::hitmarker) {
                    std::lock_guard<std::mutex> lock(g_hit_mutex);
                    g_HitMarkers.push_back({ data.head_pos, damage, (float)GetTickCount64() / 1000.f, 1.0f });
                    
                    if (Settings::hitsound && !Settings::hitsound_files.empty() && Settings::hitsound_files[0] != "No sounds found") {
                        char cmd[512];
                        mciSendStringA("close hithard", NULL, 0, NULL); 
                        sprintf_s(cmd, "open \"%s\\sound\\hit\\%s\" alias hithard", Settings::exeDir.c_str(), Settings::hitsound_files[Settings::hitsound_sel].c_str());
                        mciSendStringA(cmd, NULL, 0, NULL);
                        mciSendStringA("play hithard from 0", NULL, 0, NULL);
                    }
                }
            }
            s_LastHP[i] = current_hp;
        }

        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            g_front_buffer = g_back_buffer;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ────────────────────────────────────────────
//  ESP Draw (Rendering Ported from Khytt)
// ────────────────────────────────────────────
static void DrawCornerBox(ImDrawList* dl, float x, float y, float w, float h, ImU32 col, float t = 2.f) {
    float cw = w * 0.25f;
    float ch = h * 0.20f;
    dl->AddLine({x,y}, {x+cw,y}, col, t); dl->AddLine({x,y}, {x,y+ch}, col, t);
    dl->AddLine({x+w,y}, {x+w-cw,y}, col, t); dl->AddLine({x+w,y}, {x+w,y+ch}, col, t);
    dl->AddLine({x,y+h}, {x+cw,y+h}, col, t); dl->AddLine({x,y+h}, {x,y+h-ch}, col, t);
    dl->AddLine({x+w,y+h}, {x+w-cw,y+h}, col, t); dl->AddLine({x+w,y+h}, {x+w,y+h-ch}, col, t);
}

static void Draw3DBox(ImDrawList* dl, Vector3 base, Vector3 top, const view_matrix_t& vm, float sw, float sh, ImU32 col, float thickness = 1.0f) {
    float width = 20.0f; // Fixed width for players
    Vector3 corners[8] = {
        { base.x - width, base.y - width, base.z }, { base.x + width, base.y - width, base.z },
        { base.x + width, base.y + width, base.z }, { base.x - width, base.y + width, base.z },
        { base.x - width, base.y - width, top.z },  { base.x + width, base.y - width, top.z },
        { base.x + width, base.y + width, top.z },  { base.x - width, base.y + width, top.z }
    };
    Vector2 s[8];
    bool valid[8];
    for(int i=0; i<8; i++) valid[i] = WorldToScreen(corners[i], s[i], vm, sw, sh);
    for(int i=0; i<4; i++) {
        if(valid[i] && valid[(i+1)%4]) dl->AddLine({s[i].x,s[i].y}, {s[(i+1)%4].x,s[(i+1)%4].y}, col, thickness);
        if(valid[i+4] && valid[((i+1)%4)+4]) dl->AddLine({s[i+4].x, s[i+4].y}, {s[((i+1)%4)+4].x, s[((i+1)%4)+4].y}, col, thickness);
        if(valid[i] && valid[i+4]) dl->AddLine({s[i].x, s[i].y}, {s[i+4].x, s[i+4].y}, col, thickness);
    }
}

// ────────────────────────────────────────────
//  Fake Glow (Screen-drawn RGB skeleton glow)
// ────────────────────────────────────────────
static void DrawFakeGlow(ImDrawList* dl, const ESPData& e, const view_matrix_t& vm, float sw, float sh) {
    if (!Settings::fake_glow_enemies) return;

    float r, g, b;
    GetRainbowFloat(3.0f, r, g, b);
    int ir = (int)(r * 255);
    int ig = (int)(g * 255);
    int ib = (int)(b * 255);

    ImU32 col_glow = IM_COL32(ir, ig, ib, 80);
    ImU32 col_core = IM_COL32(ir, ig, ib, 240);

    static const int skeletonPairs[][2] = {
        {6,5},{5,4},{5,13},{13,14},{14,15},
        {5,8},{8,9},{9,10},
        {4,25},{25,26},{26,27},
        {4,22},{22,23},{23,24}
    };

    for (auto& p : skeletonPairs) {
        Vector3 b1 = e.bones[p[0]].pos;
        Vector3 b2 = e.bones[p[1]].pos;
        if (b1.IsZero() || b2.IsZero()) continue;

        Vector2 s1, s2;
        if (!WorldToScreen(b1, s1, vm, sw, sh)) continue;
        if (!WorldToScreen(b2, s2, vm, sw, sh)) continue;

        // 2 clean layers - no black blocks
        dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, col_glow, 6.0f * Settings::glow_spread);
        dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, col_core, 1.5f);
    }

}

static void DrawGrenadePrediction(ImDrawList* dl, const view_matrix_t& vm, float sw, float sh) {
    if (!Settings::grenade_prediction) return;
    uintptr_t lp = Game::GetLocalPlayerPawn();
    if (!lp) return;

    uintptr_t wpn_svc = g_Memory->Read<uintptr_t>(lp + g_Off.m_pWeaponServices);
    uint32_t active_wpn_h = g_Memory->Read<uint32_t>(wpn_svc + g_Off.m_hActiveWeapon);
    uintptr_t wpn = Game::GetEntityFromHandle(active_wpn_h);
    if (!wpn) return;

    uint16_t item_idx = g_Memory->Read<uint16_t>(wpn + g_Off.m_iItemDefinitionIndex);
    // Grenade IDs: 43 (Flash), 44 (HE), 45 (Smoke), 46 (Moli), 47 (Decoy), 48 (Inc)
    if (item_idx < 43 || item_idx > 48) return;

    bool pin_pulled = g_Memory->Read<bool>(wpn + g_Off.m_bPinPulled);
    if (!pin_pulled) return;

    float throw_strength = g_Memory->Read<float>(wpn + g_Off.m_flThrowStrength);
    
    // Physics constants (Catalyst)
    const float tick_interval = 1.0f / 64.0f;
    const float gravity_scale = 0.4f;
    const float elasticity = 0.45f;
    const float sv_gravity = 800.0f; // Default CS2 gravity

    // Detonation settings based on weapon
    float det_time = 1.5f;
    float vel_threshold = 0.1f;
    bool timer_based = true;

    if (item_idx == 46 || item_idx == 48) { // Moli/Inc
        det_time = 2.0f;
        vel_threshold = 0.0f;
    } else if (item_idx == 47) { // Decoy
        det_time = 2.0f;
        vel_threshold = 0.2f;
        timer_based = false;
    } else if (item_idx == 45) { // Smoke
        det_time = 1.5f;
        vel_threshold = 0.1f;
        timer_based = false;
    }

    // Initial position (Eye pos)
    uintptr_t scene = g_Memory->Read<uintptr_t>(lp + g_Off.m_pGameSceneNode);
    Vector3 pos = g_Memory->Read<Vector3>(scene + g_Off.m_nodeAbsOrigin);
    pos.z += (throw_strength * 12.0f - 12.0f) + 64.f; // Eye height + strength offset

    // Initial velocity
    Vector3 angles = g_Memory->Read<Vector3>(g_Client + g_Off.dwViewAngles);
    
    // Catalyst angle adjustment
    if (angles.x > 90.0f) angles.x -= 360.0f;
    else if (angles.x < -90.0f) angles.x += 360.0f;
    angles.x -= (90.0f - abs(angles.x)) * 10.0f / 90.0f;

    float pitch = angles.x * ((float)3.1415926535 / 180.f);
    float yaw = angles.y * ((float)3.1415926535 / 180.f);

    Vector3 forward;
    forward.x = cosf(pitch) * cosf(yaw);
    forward.y = cosf(pitch) * sinf(yaw);
    forward.z = -sinf(pitch);

    float throw_vel = 750.0f; // Approximated max throw velocity
    float throw_speed = (throw_strength * 0.7f + 0.3f) * throw_vel;

    Vector3 lp_vel = g_Memory->Read<Vector3>(lp + g_Off.m_vecAbsVelocity);
    Vector3 vel = forward * throw_speed + lp_vel * 1.25f;

    Vector3 prev_pos = pos;
    ImU32 col = GetRainbow(2.0f);

    for (int tick = 0; tick < 128; tick++) {
        // Step simulation
        Vector3 gravity_vec = { 0, 0, -(sv_gravity * gravity_scale) * tick_interval };
        Vector3 next_pos = pos + vel * tick_interval;
        vel = vel + gravity_vec;

        // Simplified collision (no BVH externally, so we check Z)
        if (next_pos.z <= -100.f) { // Ground check
            next_pos.z = -100.f;
            vel.z = -vel.z * elasticity;
            vel.x *= elasticity;
            vel.y *= elasticity;
        }

        Vector2 s1, s2;
        if (WorldToScreen(pos, s1, vm, sw, sh) && WorldToScreen(next_pos, s2, vm, sw, sh)) {
            dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, col, 2.0f);
        }

        pos = next_pos;

        // Check detonation
        float current_time = (float)tick * tick_interval;
        if (timer_based) {
            if (current_time >= det_time) break;
        } else {
            float speed_2d = sqrtf(vel.x * vel.x + vel.y * vel.y);
            if (speed_2d < vel_threshold && tick > 10) break;
        }

        if (pos.z < -100.f && abs(vel.z) < 1.0f) break;
    }
}


inline void DrawHitMarkers(ImDrawList* dl, const view_matrix_t& vm, float sw, float sh) {
    if (!Settings::hitmarker) return;

    float current_time = (float)GetTickCount64() / 1000.f;
    std::lock_guard<std::mutex> lock(g_hit_mutex);

    for (auto it = g_HitMarkers.begin(); it != g_HitMarkers.end();) {
        float delta = current_time - it->time;
        if (delta > 1.5f) {
            it = g_HitMarkers.erase(it);
            continue;
        }

        it->alpha = 1.0f - (delta / 1.5f);
        Vector3 render_pos = it->pos;
        render_pos.z += delta * 25.0f; // Floating effect

        Vector2 screen;
        if (WorldToScreen(render_pos, screen, vm, sw, sh)) {
            char buf[16];
            sprintf_s(buf, "-%d", it->damage);
            
            // Render with highlighted bold effect and custom size
            ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(it->alpha * 255));
            ImU32 textCol   = IM_COL32(255, 40, 40, (int)(it->alpha * 255));
            if (it->damage > 50) textCol = IM_COL32(255, 255, 0, (int)(it->alpha * 255)); 

            float sz = Settings::hitmarker_size;
            
            // Draw outline/shadow in 8 directions for maximum highlight
            for (float ox = -sz; ox <= sz; ox += sz) {
                for (float oy = -sz; oy <= sz; oy += sz) {
                    if (ox == 0 && oy == 0) continue;
                    dl->AddText({ screen.x + ox, screen.y + oy }, shadowCol, buf);
                }
            }
            
            // Main text
            dl->AddText({ screen.x, screen.y }, textCol, buf);
        }
        ++it;
    }
}

inline void DrawESP() {
    if (!Settings::esp_enabled) return;

    auto* dl = ImGui::GetBackgroundDrawList();
    float sw = ImGui::GetIO().DisplaySize.x;
    float sh = ImGui::GetIO().DisplaySize.y;
    view_matrix_t vm = Game::GetViewMatrix();

    DrawGrenadePrediction(dl, vm, sw, sh);
    DrawHitMarkers(dl, vm, sw, sh);

    std::vector<ESPData> ents;
    {
        std::lock_guard<std::mutex> lk(g_data_mutex);
        ents = g_front_buffer;
    }

    for (auto& e : ents) {
        if (!e.active) continue;

        // Draw fake glow first (behind everything else)
        DrawFakeGlow(dl, e, vm, sw, sh);

        Vector2 sFeet, sHead;
        if (!WorldToScreen(e.feet_pos, sFeet, vm, sw, sh) || !WorldToScreen(e.head_pos, sHead, vm, sw, sh)) continue;


        float h = sFeet.y - sHead.y;
        float w = h * 0.45f;
        float x = sHead.x - w / 2.f;
        float y = sHead.y;

        ImU32 boxCol = (Settings::rainbow_global || Settings::rainbow_box) ? GetRainbow(1.f, 0.9f) : 
                       IM_COL32((int)(Settings::box_color[0]*255), (int)(Settings::box_color[1]*255), 
                                (int)(Settings::box_color[2]*255), (int)(Settings::box_color[3]*255));

        if (Settings::box_2d) {
            if (Settings::fill_box) dl->AddRectFilled({x,y}, {x+w,y+h}, IM_COL32(0,0,0,30));
            if (Settings::outline_box) {
                dl->AddRect({x-1,y-1}, {x+w+1,y+h+1}, IM_COL32(0,0,0,120), 0, 0, 1.2f);
            }
            if (Settings::box_style == 0) dl->AddRect({x,y}, {x+w,y+h}, boxCol, 0, 0, 1.0f);
            else if (Settings::box_style == 1) DrawCornerBox(dl, x, y, w, h, boxCol);
        }

        if (Settings::box_3d) {
            Draw3DBox(dl, e.feet_pos, e.head_pos, vm, sw, sh, boxCol);
        }

        if (Settings::health_bar) {
            float hpFrac = e.health / 100.f;
            ImU32 hpCol = IM_COL32((int)((1-hpFrac)*255), (int)(hpFrac*220), 0, 255);
            dl->AddRectFilled({x-6,y}, {x-2,y+h}, IM_COL32(0,0,0,160));
            dl->AddRectFilled({x-5,y+h*(1-hpFrac)}, {x-3,y+h}, hpCol);
        }

        if (Settings::armor_bar) {
            float armFrac = e.armor / 100.f;
            ImU32 armCol = IM_COL32(0, 150, 255, 255); // CS Blue
            dl->AddRectFilled({x+w+2,y}, {x+w+6,y+h}, IM_COL32(0,0,0,160));
            dl->AddRectFilled({x+w+3,y+h*(1-armFrac)}, {x+w+5,y+h}, armCol);
        }

        if (Settings::skeleton) {
            static const int skeletonPairs[][2] = {
                {6,5},{5,4},{5,13},{13,14},{14,15},{5,8},{8,9},{9,10},
                {4,25},{25,26},{26,27}, // Right leg to foot
                {4,22},{22,23},{23,24}  // Left leg to foot
            };
            ImU32 boneCol = Settings::rainbow_global ? GetRainbow() : IM_COL32(255,255,255,200);
            
            // --- External Fake Chams (Polygon Fill) ---
            if (Settings::fake_chams) {
                // Drawing a solid-filled torso/head area
                Vector2 sHead, sNeck, sPelvis, sLShoulder, sRShoulder, sLHip, sRHip;
                bool v1 = WorldToScreen(e.bones[6].pos, sHead, vm, sw, sh);      // Head
                bool v2 = WorldToScreen(e.bones[5].pos, sNeck, vm, sw, sh);      // Neck
                bool v3 = WorldToScreen(e.bones[4].pos, sPelvis, vm, sw, sh);    // Pelvis
                bool v4 = WorldToScreen(e.bones[13].pos, sLShoulder, vm, sw, sh); // L Shoulder
                bool v5 = WorldToScreen(e.bones[8].pos, sRShoulder, vm, sw, sh);  // R Shoulder
                bool v6 = WorldToScreen(e.bones[22].pos, sLHip, vm, sw, sh);      // L Hip
                bool v7 = WorldToScreen(e.bones[25].pos, sRHip, vm, sw, sh);      // R Hip

                if (v1 && v2 && v3) {
                    ImU32 fillCol = IM_COL32(255, 255, 255, 255); // Solid White
                    ImU32 outCol = IM_COL32(0, 0, 0, 255);        // Black Outline
                    
                    // Draw a "blob" or "capsule" to represent the body
                    float bodyW = h * 0.25f;
                    
                    // Simplified: draw a few filled shapes for the main body parts
                    // Torso
                    if (v2 && v3 && v4 && v5) {
                        dl->AddQuadFilled({sLShoulder.x, sLShoulder.y}, {sRShoulder.x, sRShoulder.y}, 
                                          {sRHip.x, sRHip.y}, {sLHip.x, sLHip.y}, fillCol);
                        dl->AddQuad({sLShoulder.x, sLShoulder.y}, {sRShoulder.x, sRShoulder.y}, 
                                    {sRHip.x, sRHip.y}, {sLHip.x, sLHip.y}, outCol, 1.5f);
                    }
                    
                    // Head circle
                    dl->AddCircleFilled({sHead.x, sHead.y}, h * 0.08f, fillCol);
                    dl->AddCircle({sHead.x, sHead.y}, h * 0.08f, outCol, 1.5f);
                    
                    // Thicker arms/legs for the "filled" look
                    for (auto& p : skeletonPairs) {
                        Vector3 b1 = e.bones[p[0]].pos;
                        Vector3 b2 = e.bones[p[1]].pos;
                        Vector2 s1, s2;
                        if (WorldToScreen(b1, s1, vm, sw, sh) && WorldToScreen(b2, s2, vm, sw, sh)) {
                             dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, fillCol, 4.0f);
                             dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, outCol, 5.5f); // Outline
                             dl->AddLine({s1.x, s1.y}, {s2.x, s2.y}, fillCol, 4.0f); // Re-draw core
                        }
                    }
                }
            } else {
                // Normal Skeleton
                for (auto& p : skeletonPairs) {
                    Vector3 b1 = e.bones[p[0]].pos;
                    Vector3 b2 = e.bones[p[1]].pos;
                    Vector2 s1, s2;
                    if (WorldToScreen(b1, s1, vm, sw, sh) && WorldToScreen(b2, s2, vm, sw, sh))
                        dl->AddLine({s1.x,s1.y}, {s2.x,s2.y}, boneCol, 1.2f);
                }
            }
        }
        if (Settings::head_dot) {
            ImU32 dotCol = Settings::rainbow_global ? GetRainbow() : IM_COL32(255, 0, 0, 255);
            dl->AddCircleFilled({sHead.x, sHead.y}, (int)(h * 0.05f), dotCol);
        }

        if (Settings::snaplines) {
            ImU32 lineCol = Settings::rainbow_global ? GetRainbow() : IM_COL32(255,255,255,150);
            dl->AddLine({sw/2.f, sh}, {sFeet.x, sFeet.y}, lineCol, 1.0f);
        }

        if (Settings::names) {
            ImU32 txtCol = Settings::rainbow_global ? GetRainbow() : IM_COL32(255,255,255,255);
            float nameX = x + w / 2.f - ImGui::CalcTextSize(e.name).x / 2.f;
            float nameY = y - 14.f;
            dl->AddText({nameX, nameY}, txtCol, e.name[0] ? e.name : "Player");

            if (Settings::avatar_esp && e.steam_id != 0) {
                ID3D11ShaderResourceView* avatar = AvatarManager::Get().GetAvatar(e.steam_id);
                if (avatar) {
                    dl->AddImage((void*)avatar, {nameX - 16.f, nameY}, {nameX - 2.f, nameY + 14.f});
                }
            }
        }

        if (Settings::distance) {
            char distStr[32]; snprintf(distStr, sizeof(distStr), "[%dm]", (int)e.distance);
            dl->AddText({x + w/2.f - ImGui::CalcTextSize(distStr).x/2.f, y + h + 2.f}, IM_COL32(255, 255, 255, 200), distStr);
        }

    }

    if (Settings::watermark) {
        static float last_fps = 0.f;
        static uint64_t last_tick = 0;
        uint64_t current_tick = GetTickCount64();
        if (current_tick - last_tick > 500) {
            last_fps = ImGui::GetIO().Framerate;
            last_tick = current_tick;
        }

        char wm[128]; 
        snprintf(wm, sizeof(wm), "LUMEN | %d ents | %.0f fps", (int)ents.size(), last_fps);
        
        ImVec2 ts = ImGui::CalcTextSize(wm);
        ImVec2 p = ImVec2(15, 15);
        ImVec2 sz = ImVec2(ts.x + 25, ts.y + 12);

        // Get current rainbow color
        ImU32 rainbow = GetRainbow(1.5f);

        // Glassmorphism background
        dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(10, 10, 15, 220), 6.f);
        
        // RGB Border
        dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), rainbow, 6.f, 0, 1.0f);
        
        // Shadow text
        dl->AddText(ImVec2(p.x + 13, p.y + 7), IM_COL32(0, 0, 0, 200), wm);
        
        // RGB Rainbow text
        dl->AddText(ImVec2(p.x + 12, p.y + 6), rainbow, wm);
    }
}
