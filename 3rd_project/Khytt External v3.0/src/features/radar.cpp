#include "radar.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>
#include <cmath>
#include <imgui.h>

void Visuals::DrawRadar() {
  if (!Settings::visual_radar)
    return;

  ImGui::SetNextWindowSize(ImVec2(Settings::radar_size, Settings::radar_size),
                           ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Radar", &Settings::visual_radar,
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImVec2 center(win_pos.x + win_size.x / 2.0f, win_pos.y + win_size.y / 2.0f);

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y),
        IM_COL32(30, 30, 30, 200));
    draw_list->AddLine(ImVec2(win_pos.x, center.y),
                       ImVec2(win_pos.x + win_size.x, center.y),
                       IM_COL32(100, 100, 100, 255));
    draw_list->AddLine(ImVec2(center.x, win_pos.y),
                       ImVec2(center.x, win_pos.y + win_size.y),
                       IM_COL32(100, 100, 100, 255));

    uintptr_t local_player = Game::GetLocalPlayerPawn();
    if (!local_player) {
      ImGui::End();
      return;
    }

    Vector3 local_pos = Memory::Read<Vector3>(
        local_player +
        cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
    Vector3 local_angles = Memory::Read<Vector3>(
        local_player +
        cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles);
    int local_team = Memory::Read<int>(
        local_player +
        cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

    for (int i = 1; i <= 64; i++) {
      uintptr_t controller = Game::GetPlayerController(i);
      if (!controller)
        continue;

      uintptr_t player = Game::GetPawnFromController(controller);
      if (!player || player == local_player)
        continue;

      int health = Memory::Read<int>(
          player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
      if (health <= 0 || health > 100)
        continue;

      Vector3 pos = Memory::Read<Vector3>(
          player +
          cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
      int team = Memory::Read<int>(
          player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

      float rel_x = (pos.x - local_pos.x) / Settings::radar_scale;
      float rel_y = (pos.y - local_pos.y) / Settings::radar_scale;

      if (Settings::radar_rotating) {
        float angle = (local_angles.y - 90.0f) * (3.14159265f / 180.0f);
        float s = sin(angle);
        float c = cos(angle);
        float tmp_x = rel_x;
        rel_x = tmp_x * c - rel_y * s;
        rel_y = tmp_x * s + rel_y * c;
      }

      ImVec2 point_pos(center.x + rel_x, center.y - rel_y);

      // Clamp to radar window
      if (point_pos.x < win_pos.x)
        point_pos.x = win_pos.x;
      if (point_pos.x > win_pos.x + win_size.x)
        point_pos.x = win_pos.x + win_size.x;
      if (point_pos.y < win_pos.y)
        point_pos.y = win_pos.y;
      if (point_pos.y > win_pos.y + win_size.y)
        point_pos.y = win_pos.y + win_size.y;

      ImU32 color = (team == local_team) ? IM_COL32(0, 255, 0, 255)
                                         : IM_COL32(255, 0, 0, 255);
      draw_list->AddCircleFilled(point_pos, 3.0f, color);
    }
  }
  ImGui::End();
}
