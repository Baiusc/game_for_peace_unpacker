#pragma once
#include <Windows.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include "imgui/imgui.h"
#include "../SDK/Game.h"
#include "../SDK/Structs.h"

namespace Settings {
    extern bool menu_visible;
    extern bool bomb_esp;
    extern bool bomb_timer_hud;
    extern bool spectator_list;
    extern bool hitsound;
}

// ────────────────────────────────────────────
//  Bomb Info (Ported & Enhanced from Khytt)
// ────────────────────────────────────────────
// ────────────────────────────────────────────
//  Bomb HUD (Redesigned with Modern Aesthetics)
// ────────────────────────────────────────────
inline void DrawBombInfo() {
    if (!Settings::bomb_timer_hud) return;

    uintptr_t c4_ptr = g_Memory->Read<uintptr_t>(g_Client + g_Off.dwPlantedC4);
    if (!c4_ptr || c4_ptr < 0x1000) return;

    bool ticking = g_Memory->Read<bool>(c4_ptr + g_Off.m_bBombTicking);
    if (!ticking) return;

    float blowTime = g_Memory->Read<float>(c4_ptr + g_Off.m_flC4Blow);
    float timerLength = g_Memory->Read<float>(c4_ptr + g_Off.m_flTimerLength);
    bool defusing = g_Memory->Read<bool>(c4_ptr + g_Off.m_bBeingDefused);
    float defuseFinish = g_Memory->Read<float>(c4_ptr + g_Off.m_flDefuseCountDown);
    float defuseLength = g_Memory->Read<float>(c4_ptr + g_Off.m_flDefuseLength);
    int site = g_Memory->Read<int>(c4_ptr + g_Off.m_nBombSite);
    
    // Try to get current time from GlobalVars
    GlobalVars g_vars = Game::GetGlobalVars();
    float time_left = blowTime - g_vars.current_time;
    float defuse_left = defuseFinish - g_vars.current_time;
    
    // Safety check: if time_left is impossible, try to use a fallback or display nothing
    if (time_left < 0 || time_left > 60.0f) {
        if (time_left < 0) time_left = 0;
        else return; 
    }
    if (defuse_left < 0) defuse_left = 0;

    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    
    // Glassmorphism Window for Bomb
    ImGui::SetNextWindowPos(ImVec2(sw / 2 - 125, 80), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(250, 0));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.2f, 0.2f, 0.4f));

    if (ImGui::Begin("BombHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize)) {
        const char* site_name = (site == 1) ? "SITE B" : "SITE A";
        
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x/2 - ImGui::CalcTextSize(site_name).x/2);
        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), site_name);
        
        char time_buf[32];
        sprintf_s(time_buf, "%.2fs", time_left);
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x/2 - ImGui::CalcTextSize(time_buf).x/2);
        ImGui::TextColored(ImVec4(1, 1, 1, 1), time_buf);

        float progress = std::clamp(time_left / (timerLength > 0.f ? timerLength : 40.0f), 0.0f, 1.0f);
        
        // Gradient color for progress bar
        ImVec4 barCol = ImVec4(1.0f - progress, progress, 0.2f, 1.0f);
        if (time_left < 10.0f) barCol = ImVec4(1, 0.1f, 0.1f, 1);
        if (defusing && defuse_left > time_left) barCol = ImVec4(1, 0, 0, 1);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barCol);
        ImGui::ProgressBar(progress, ImVec2(230, 12), "");
        ImGui::PopStyleColor();

        if (defusing) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.2f, 0.7f, 1, 1), "DEFUSING... %.2fs", defuse_left);
            float defuse_prog = std::clamp(1.0f - (defuse_left / (defuseLength > 0.f ? defuseLength : 5.0f)), 0.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0, 0.6f, 1, 1));
            ImGui::ProgressBar(defuse_prog, ImVec2(230, 8), "");
            ImGui::PopStyleColor();
            
            if (defuse_left > time_left) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "!!! CANNOT DEFUSE !!!");
            } else {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "--- DEFUSE OK ---");
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

// ────────────────────────────────────────────
//  Bomb ESP (World-space rendering)
// ────────────────────────────────────────────
inline void DrawBombESP() {
    if (!Settings::bomb_esp) return;

    uintptr_t c4 = g_Memory->Read<uintptr_t>(g_Client + g_Off.dwPlantedC4);
    if (!c4) return;

    bool ticking = g_Memory->Read<bool>(c4 + g_Off.m_bBombTicking);
    if (!ticking) return;

    uintptr_t scene = g_Memory->Read<uintptr_t>(c4 + g_Off.m_pGameSceneNode);
    Vector3 pos = g_Memory->Read<Vector3>(scene + g_Off.m_nodeAbsOrigin);
    
    view_matrix_t vm = Game::GetViewMatrix();
    float sw = ImGui::GetIO().DisplaySize.x;
    float sh = ImGui::GetIO().DisplaySize.y;
    
    Vector2 sPos;
    if (WorldToScreen(pos, sPos, vm, sw, sh)) {
        auto* dl = ImGui::GetBackgroundDrawList();
        
        // 3D Box on bomb
        Vector3 top = pos; top.z += 15.f;
        float width = 8.0f;
        Vector3 corners[8] = {
            { pos.x - width, pos.y - width, pos.z }, { pos.x + width, pos.y - width, pos.z },
            { pos.x + width, pos.y + width, pos.z }, { pos.x - width, pos.y + width, pos.z },
            { pos.x - width, pos.y - width, top.z },  { pos.x + width, pos.y - width, top.z },
            { pos.x + width, pos.y + width, top.z },  { pos.x - width, pos.y + width, top.z }
        };
        Vector2 s[8]; bool valid[8];
        for(int i=0; i<8; i++) valid[i] = WorldToScreen(corners[i], s[i], vm, sw, sh);
        
        ImU32 col = IM_COL32(255, 50, 50, 200);
        for(int i=0; i<4; i++) {
            if(valid[i] && valid[(i+1)%4]) dl->AddLine({s[i].x,s[i].y}, {s[(i+1)%4].x,s[(i+1)%4].y}, col, 1.5f);
            if(valid[i+4] && valid[((i+1)%4)+4]) dl->AddLine({s[i+4].x, s[i+4].y}, {s[((i+1)%4)+4].x, s[((i+1)%4)+4].y}, col, 1.5f);
            if(valid[i] && valid[i+4]) dl->AddLine({s[i].x, s[i].y}, {s[i+4].x, s[i+4].y}, col, 1.5f);
        }

        // Label
        int site = g_Memory->Read<int>(c4 + g_Off.m_nBombSite);
        char buf[64];
        sprintf_s(buf, "PLANTED C4 [%s]", (site == 1) ? "B" : "A");
        
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({sPos.x - ts.x/2, sPos.y - 40}, IM_COL32(255,255,255,255), buf);
    }
}

// ────────────────────────────────────────────
//  Spectator List
// ────────────────────────────────────────────
inline void DrawSpectatorList() {
    if (!Settings::spectator_list) return;

    std::vector<ESPData> ents;
    {
        std::lock_guard<std::mutex> lk(g_data_mutex);
        ents = g_front_buffer;
    }

    std::vector<std::string> spectators;
    for (const auto& e : ents) {
        if (e.active && e.is_spectating_local) {
            spectators.push_back(e.name[0] ? e.name : "Player");
        }
    }

    if (spectators.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 210, 50), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(200, 0));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.09f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.45f, 0.95f, 0.5f));
    
    if (ImGui::Begin("Spectator List", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
        if (spectators.empty()) {
            ImGui::TextDisabled("No spectators");
        } else {
            for (const auto& name : spectators) {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", name.c_str());
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

