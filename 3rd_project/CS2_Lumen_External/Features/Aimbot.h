#pragma once
#include <Windows.h>
#include <cmath>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>

#include "ESP.h" 
#include "../SDK/Game.h"
#include "../SDK/Structs.h"

// External declarations from Main.cpp
extern std::atomic<bool> g_Running;
extern Memory*   g_Memory;
extern uintptr_t g_Client;

namespace Settings {
    struct WeaponConfig {
        bool  enabled = true;
        float fov     = 5.0f;
        float smooth  = 10.0f;
        int   bone    = 0;
    };
    extern WeaponConfig weapon_configs[4];
    extern bool  aim_enabled;
    extern bool  target_legit;
    extern bool  safety_lock;
    extern bool  aim_vis_check;
    extern bool  aim_autowall;
    extern bool  aimbot_humanized;
    extern float aim_jitter;
    extern float sensitivity;
    extern int   aim_key;
    extern bool  target_lock_prevent;
    extern bool  lock_prevent_active;
    extern bool  rcs_standalone;
    extern float rcs_scale;
}

// ────────────────────────────────────────────
//  Aimbot Logic (Fully ported from Khytt v2.7)
// ────────────────────────────────────────────
inline void AimbotThread() {
    float last_punch_x = 0.f;
    float last_punch_y = 0.f;
    int last_target_idx = -1;

    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (!Settings::aim_enabled || !g_Client) continue;

        uintptr_t local_pawn = Game::GetLocalPlayerPawn();
        if (!local_pawn) continue;

        // --- Weapon Detection ---
        uintptr_t active_weapon = Game::GetActiveWeapon(local_pawn);
        int weapon_id = Game::GetItemDefinitionIndex(active_weapon);
        int weapon_type = Game::GetWeaponType(weapon_id);
        
        Settings::WeaponConfig& cfg = Settings::weapon_configs[weapon_type];
        if (!cfg.enabled) {
            // Even if aimbot is disabled for this weapon, we might still want RCS
            if (Settings::rcs_standalone) {
                Vector3 punch = g_Memory->Read<Vector3>(local_pawn + g_Off.m_aimPunchAngle);
                float dx = punch.y - last_punch_x;
                float dy = punch.x - last_punch_y;
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
                        float m_yaw = 0.022f * Settings::sensitivity;
                        float x_move = (dx * 2.f / m_yaw) * Settings::rcs_scale;
                        float y_move = (dy * 2.f / m_yaw) * Settings::rcs_scale;
                        mouse_event(MOUSEEVENTF_MOVE, (DWORD)(int)-x_move, (DWORD)(int)y_move, 0, 0);
                    }
                }
                last_punch_x = punch.y;
                last_punch_y = punch.x;
            }
            continue;
        }

        bool key_held = (GetAsyncKeyState(Settings::aim_key) & 0x8000) != 0;

        // --- Lock Prevent Reset ---
        if (!key_held) {
            Settings::lock_prevent_active = false;
        }

        if (Settings::target_lock_prevent && Settings::lock_prevent_active)
            continue;

        // --- RCS ---
        if (Settings::rcs_standalone) {
            Vector3 punch = g_Memory->Read<Vector3>(local_pawn + g_Off.m_aimPunchAngle);
            float dx = punch.y - last_punch_x;
            float dy = punch.x - last_punch_y;

            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
                    float m_yaw = 0.022f * Settings::sensitivity;
                    float x_move = (dx * 2.f / m_yaw) * Settings::rcs_scale;
                    float y_move = (dy * 2.f / m_yaw) * Settings::rcs_scale;
                    mouse_event(MOUSEEVENTF_MOVE, (DWORD)(int)-x_move, (DWORD)(int)y_move, 0, 0);
                }
            }
            last_punch_x = punch.y;
            last_punch_y = punch.x;
        }

        if (!key_held) continue;

        // --- Target Selection ---
        uintptr_t scene_node = g_Memory->Read<uintptr_t>(local_pawn + g_Off.m_pGameSceneNode);
        Vector3 local_origin = g_Memory->Read<Vector3>(scene_node + g_Off.m_nodeAbsOrigin);
        Vector3 view_offset = g_Memory->Read<Vector3>(local_pawn + g_Off.m_vecViewOffset);
        Vector3 eye_pos = local_origin + view_offset;

        Vector3 current_angles = g_Memory->Read<Vector3>(g_Client + g_Off.dwViewAngles);
        int local_team = g_Memory->Read<int>(local_pawn + g_Off.m_iTeamNum);

        float best_fov = cfg.fov;
        Vector3 best_target_angles = { 0.f, 0.f, 0.f };
        int best_target_index = -1;

        uintptr_t ent_list = Game::GetEntityList();
        for (int i = 1; i <= 64; i++) {
            uintptr_t controller = Game::GetPlayerController(ent_list, i);
            if (!controller) continue;

            uintptr_t pawn_ent = Game::GetPawnFromController(controller, ent_list);
            if (!pawn_ent || pawn_ent == local_pawn) continue;

            int health = g_Memory->Read<int>(pawn_ent + g_Off.m_iHealth);
            if (health <= 0 || health > 100) continue;

            int team = g_Memory->Read<int>(pawn_ent + g_Off.m_iTeamNum);
            if (team == local_team) continue;

            if (Settings::aim_vis_check && !Settings::aim_autowall) {
                uint32_t spotted = g_Memory->Read<uint32_t>(pawn_ent + g_Off.m_entitySpottedState + g_Off.m_bSpotted);
                if (spotted == 0) continue;
            }

            int bone_id = 6;
            switch (cfg.bone) {
                case 1: bone_id = 5; break;
                case 2: bone_id = 4; break;
                case 3: bone_id = 2; break;
                default: bone_id = 6; break;
            }

            uintptr_t bone_matrix = Game::GetBoneMatrix(pawn_ent);
            Vector3 target_pos = Game::GetBonePos(bone_matrix, bone_id);
            if (target_pos.IsZero()) {
                uintptr_t t_scene = g_Memory->Read<uintptr_t>(pawn_ent + g_Off.m_pGameSceneNode);
                target_pos = g_Memory->Read<Vector3>(t_scene + g_Off.m_nodeAbsOrigin);
                target_pos.z += 65.f;
            }

            Vector3 relative_pos = target_pos - eye_pos;
            Vector3 ideal_angles = relative_pos.ToAngle();

            if (ideal_angles.x > 89.f)  ideal_angles.x = 89.f;
            if (ideal_angles.x < -89.f) ideal_angles.x = -89.f;

            float delta_yaw   = ideal_angles.y - current_angles.y;
            float delta_pitch = ideal_angles.x - current_angles.x;

            while (delta_yaw > 180.f)  delta_yaw -= 360.f;
            while (delta_yaw < -180.f) delta_yaw += 360.f;

            float fov = sqrtf(delta_yaw * delta_yaw + delta_pitch * delta_pitch);
            if (fov < best_fov) {
                best_fov = fov;
                best_target_angles = ideal_angles;
                best_target_index = i;
            }
        }

        // --- Lock Prevent Logic ---
        if (Settings::target_lock_prevent) {
            if (last_target_idx != -1 && best_target_index != last_target_idx) {
                uintptr_t ctrl = Game::GetPlayerController(ent_list, last_target_idx);
                uintptr_t pawn = Game::GetPawnFromController(ctrl, ent_list);
                int hp = g_Memory->Read<int>(pawn + g_Off.m_iHealth);
                if (hp <= 0) Settings::lock_prevent_active = true;
            }
            last_target_idx = best_target_index;
        }

        if (best_target_angles.IsZero()) continue;

        // --- Apply Aim ---
        float smooth = std::max(1.f, cfg.smooth);
        if (Settings::aimbot_humanized) {
            float r = ((float)rand() / (float)RAND_MAX) * 2.f - 1.f;
            smooth += r * (smooth * 0.2f);
            smooth = std::max(1.f, smooth);
        }

        Vector3 angle_step = best_target_angles - current_angles;
        while (angle_step.y > 180.f)  angle_step.y -= 360.f;
        while (angle_step.y < -180.f) angle_step.y += 360.f;

        if (std::abs(angle_step.x) < 0.05f && std::abs(angle_step.y) < 0.05f) continue;

        if (Settings::target_legit) {
            float m_yaw = 0.022f * Settings::sensitivity;
            if (Settings::aimbot_humanized) {
                float jx = (((float)rand() / (float)RAND_MAX) - 0.5f) * Settings::aim_jitter;
                float jy = (((float)rand() / (float)RAND_MAX) - 0.5f) * Settings::aim_jitter;
                angle_step.y += jx;
                angle_step.x += jy;
            }

            float x_move = (angle_step.y / m_yaw) / smooth;
            float y_move = (angle_step.x / m_yaw) / smooth;

            // Debug Acquire (Optional: remove after verification)
            static int last_best_idx = -1;
            if (best_target_index != last_best_idx) {
                std::cout << "[*] Aimbot targeting index: " << best_target_index << " (FOV: " << best_fov << ")" << std::endl;
                last_best_idx = best_target_index;
            }

            x_move = std::max(-40.f, std::min(40.f, x_move));
            y_move = std::max(-40.f, std::min(40.f, y_move));

            if (std::abs(x_move) < 1.f && std::abs(x_move) > 0.001f)
                x_move = (x_move > 0) ? 1.f : -1.f;
            if (std::abs(y_move) < 1.f && std::abs(y_move) > 0.001f)
                y_move = (y_move > 0) ? 1.f : -1.f;

            // FIX: Positive x_move moves mouse RIGHT
            mouse_event(MOUSEEVENTF_MOVE, (DWORD)(int)x_move, (DWORD)(int)y_move, 0, 0);
        } else {
            if (Settings::safety_lock) {
                static uint64_t last_warn = 0;
                if (GetTickCount64() - last_warn > 5000) {
                    std::cout << "[!] Memory Aimbot BLOCKED by Safety Lock." << std::endl;
                    last_warn = GetTickCount64();
                }
                continue;
            }

            Vector3 smoothed = current_angles + angle_step * (1.0f / smooth);
            if (smoothed.x > 89.f) smoothed.x = 89.f;
            if (smoothed.x < -89.f) smoothed.x = -89.f;
            while (smoothed.y > 180.f) smoothed.y -= 360.f;
            while (smoothed.y < -180.f) smoothed.y += 360.f;

            g_Memory->Write<Vector3>(g_Client + g_Off.dwViewAngles, smoothed);
        }
    }
}
