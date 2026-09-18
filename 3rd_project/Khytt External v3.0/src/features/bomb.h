#pragma once
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>

namespace Visuals {
inline void DrawBombTimer() {
  if (!Settings::visual_c4)
    return;

  uintptr_t entity_list = Game::GetEntityList();
  if (!entity_list)
    return;

  // Iterate a reasonable number of entities to find C_PlantedC4
  // In a real game, C4 is usually early in the list, but let's check up to 512
  uintptr_t c4_entity = 0;

  for (int i = 64; i < 512; i++) {
    uintptr_t list_entry =
        Memory::Read<uintptr_t>(entity_list + 0x10 + 8 * ((i & 0x7FFF) >> 9));
    if (!list_entry)
      continue;

    uintptr_t entity = Memory::Read<uintptr_t>(list_entry + 0x70 * (i & 0x1FF));
    if (!entity)
      continue;

    // Check class name / designer name
    // uintptr_t entity_identity = Memory::Read<uintptr_t>(entity + 0x10);
    // uintptr_t designer_name = Memory::Read<uintptr_t>(entity_identity +
    // 0x20); std::string name = Memory::ReadString(designer_name, 32); if (name
    // == "planted_c4") ...

    // Faster way: Check if it has m_flC4Blow property which is unique-ish or
    // just class ID if we had it. We can use the schema C_PlantedC4 to check
    // specific values or just rely on finding it.

    // Alternative: Loop and check for a specific prop.
    // Let's rely on finding "planted_c4" string in Entity Identity.

    uintptr_t entity_identity = Memory::Read<uintptr_t>(entity + 0x10);
    if (!entity_identity)
      continue;

    uintptr_t designer_name_ptr =
        Memory::Read<uintptr_t>(entity_identity + 0x20);
    if (!designer_name_ptr)
      continue;

    std::string name = Memory::ReadString(designer_name_ptr, 32);
    if (name.find("planted_c4") != std::string::npos) {
      c4_entity = entity;
      break;
    }
  }

  if (c4_entity) {
    // It's planted!
    bool is_ticking = Memory::Read<bool>(
        c4_entity +
        cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking);

    if (is_ticking) {
      float blow_time = Memory::Read<float>(
          c4_entity + cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow);

      float timer_length = Memory::Read<float>(
          c4_entity +
          cs2_dumper::schemas::client_dll::C_PlantedC4::m_flTimerLength);

      bool defusing = Memory::Read<bool>(
          c4_entity +
          cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused);

      float defuse_finish = Memory::Read<float>(
          c4_entity +
          cs2_dumper::schemas::client_dll::C_PlantedC4::m_flDefuseCountDown);

      float defuse_length = Memory::Read<float>(
          c4_entity +
          cs2_dumper::schemas::client_dll::C_PlantedC4::m_flDefuseLength);

      int site = Memory::Read<int>(
          c4_entity +
          cs2_dumper::schemas::client_dll::C_PlantedC4::m_nBombSite);
      std::string site_str = (site == 1) ? "B" : "A";

      // Calculate time remaining using GlobalVars
      GlobalVars g_vars = Game::GetGlobalVars();
      float time_left = blow_time - g_vars.current_time;
      float defuse_left = defuse_finish - g_vars.current_time;

      if (time_left < 0)
        time_left = 0;
      if (defuse_left < 0)
        defuse_left = 0;

      // Draw Window
      ImGui::SetNextWindowPos(
          ImVec2(ImGui::GetIO().DisplaySize.x / 2 - 100, 100));
      ImGui::SetNextWindowSize(ImVec2(200, 0)); // Auto height
      ImGui::Begin("Bomb", nullptr,
                   ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoBackground);

      // Site Indicator
      ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "SITE %s", site_str.c_str());
      ImGui::PopFont();

      // Explosion Timer Bar
      float progress = time_left / timer_length; // Assuming 40s timer usually
      if (progress > 1.0f)
        progress = 1.0f;

      // Color shifts from Green -> Yellow -> Red
      ImVec4 barColor = ImVec4(1.0f - progress, progress, 0.0f, 1.0f);
      if (time_left < 5.0f || (defusing && defuse_left > time_left))
        barColor = ImVec4(1, 0, 0, 1);
      if (time_left < 10.0f && !defusing)
        barColor = ImVec4(1, 0.5f, 0, 1);

      char buf[32];
      sprintf_s(buf, "%.1f s", time_left);
      ImGui::ProgressBar(progress, ImVec2(180, 15), buf);

      // Defuse Timer
      if (defusing) {
        float defuse_prog = defuse_left / defuse_length;
        ImVec4 defColor = ImVec4(0, 0.5f, 1, 1);
        if (defuse_left > time_left)
          defColor = ImVec4(1, 0, 0, 1); // No time diff

        sprintf_s(buf, "Defusing: %.1f s", defuse_left);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, defColor);
        ImGui::ProgressBar(
            1.0f - (defuse_left / defuse_length), ImVec2(180, 10),
            buf); // Inverted to fill up? Or drain. Standard is drain.
        ImGui::PopStyleColor();
      }

      ImGui::End();
    }
  }
}
} // namespace Visuals
