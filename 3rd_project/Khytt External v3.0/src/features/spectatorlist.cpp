#include "spectatorlist.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>
#include <imgui.h>
#include <string>
#include <vector>

void Visuals::DrawSpectators() {
  if (!Settings::draw_spectators)
    return;

  ImGui::SetNextWindowSize(ImVec2(200, 150), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Spectators", &Settings::draw_spectators,
                   ImGuiWindowFlags_NoCollapse)) {
    uintptr_t local_player = Game::GetLocalPlayerPawn();
    if (!local_player) {
      ImGui::End();
      return;
    }

    for (int i = 0; i < 64; i++) {
      uintptr_t controller = Game::GetPlayerController(i);
      if (!controller)
        continue;

      // Check if player is spectating
      uintptr_t observer_services = Memory::Read<uintptr_t>(
          controller + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::
                           m_pObserverServices);
      // Note: m_pObserverServices is actually in C_BasePlayerPawn usually, but
      // controllers can have it too if spectating Let's check pawn too if
      // controller doesn't have it
      if (!observer_services) {
        uintptr_t pawn = Game::GetPawnFromController(controller);
        if (pawn)
          observer_services = Memory::Read<uintptr_t>(
              pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::
                         m_pObserverServices);
      }

      if (!observer_services)
        continue;

      uint32_t target_handle = Memory::Read<uint32_t>(
          observer_services + cs2_dumper::schemas::client_dll::
                                  CPlayer_ObserverServices::m_hObserverTarget);
      if (!target_handle || target_handle == 0xFFFFFFFF)
        continue;

      int target_index = target_handle & 0x3FFF;
      uintptr_t target_controller = Game::GetPlayerController(target_index - 1);
      uintptr_t target_pawn = Game::GetPawnFromController(target_controller);

      if (target_pawn == local_player) {
        // Read name from controller
        char name[128] = {0};
        uintptr_t name_ptr = Memory::Read<uintptr_t>(
            controller + cs2_dumper::schemas::client_dll::CCSPlayerController::
                             m_sSanitizedPlayerName);

        // m_sSanitizedPlayerName is often a CUtlString which contains a pointer
        // to the char array
        if (name_ptr) {
          Memory::ReadBytes(name_ptr, name, sizeof(name));
          ImGui::Text("  > %s", name);
        } else {
          ImGui::Text("  > Player %d", i);
        }
      }
    }
  }
  ImGui::End();
}
