#include "triggerbot.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include "bsp_parser.h"
#include <Windows.h>
#include <chrono>
#include <thread>


void CombatMod::ApplyTrigger() {
  if (!Settings::trigger_active || Settings::safety_lock)
    return;

  if (Settings::target_key_code != 0 &&
      !GetAsyncKeyState(Settings::target_key_code))
    return;

  uintptr_t local_player = Game::GetLocalPlayerPawn();
  if (!local_player)
    return;

  int crosshair_id = Memory::Read<int>(
      local_player +
      cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);

  if (crosshair_id > 0) {
    uintptr_t entity_list = Game::GetEntityList();
    if (!entity_list)
      return;

    // m_iIDEntIndex gives the entity index directly in CS2, not the controller.
    uintptr_t target_pawn =
        Game::GetEntityFromHandle(crosshair_id, entity_list);
    if (!target_pawn)
      return;

    int local_team = Memory::Read<int>(
        local_player +
        cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    int target_team = Memory::Read<int>(
        target_pawn +
        cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

    if (local_team != target_team) {

      bool mapped_visible = true;
      if (BspParser::is_map_loaded) {
        Vector3 head_pos =
            Game::GetBonePos(Game::GetBoneMatrix(target_pawn), 6);
        Vector3 local_eye = Memory::Read<Vector3>(
            local_player +
            cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 view_offset = Memory::Read<Vector3>(
            local_player + cs2_dumper::schemas::client_dll::C_BaseModelEntity::
                               m_vecViewOffset);
        local_eye.x += view_offset.x;
        local_eye.y += view_offset.y;
        local_eye.z += view_offset.z;

        mapped_visible = BspParser::IsVisible(local_eye, head_pos);
      }

      if (mapped_visible) {
        if (Settings::trigger_reaction_ms > 0)
          std::this_thread::sleep_for(
              std::chrono::milliseconds(Settings::trigger_reaction_ms));

        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
      }
    }
  }
}
