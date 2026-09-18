#include "aimbot.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

void CombatMod::ApplyAimbot() {
  if (!Settings::target_enabled ||
      (Settings::safety_lock && !Settings::target_legit))
    return;

  uintptr_t local_player = Game::GetLocalPlayerPawn();
  if (!local_player)
    return;

  int local_team = Memory::Read<int>(
      local_player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
  Vector3 local_pos = Memory::Read<Vector3>(
      local_player +
      cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
  Vector3 eye_angles = Game::GetViewAngles();
  Vector3 view_offset = Memory::Read<Vector3>(
      local_player +
      cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
  Vector3 eye_pos = local_pos + view_offset;

  uintptr_t local_player_controller = Game::GetLocalPlayerController();
  int local_index = -1;
  for (int i = 0; i <= 64; i++) {
    if (Game::GetPlayerController(i) == local_player_controller) {
      local_index = i;
      break;
    }
  }

  // Fallback to team member check or similar if local_index not found
  if (local_index == -1)
    local_index = 0;

  auto &cfg = Settings::weapon_configs[Settings::current_weapon_type];
  if (!cfg.enabled)
    return;

  float best_fov = cfg.fov;
  Vector3 best_angle = {0, 0, 0};
  uintptr_t best_target = 0;

  uintptr_t entity_list = Game::GetEntityList();
  for (int i = 1; i <= 64; i++) {
    uintptr_t controller = Game::GetPlayerController(entity_list, i);
    if (!controller)
      continue;

    uintptr_t player = Game::GetPawnFromController(controller, entity_list);
    if (!player || player == local_player)
      continue;

    int team = Memory::Read<int>(
        player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    if (team == local_team)
      continue;

    int health = Memory::Read<int>(
        player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
    if (health <= 0 || health > 100)
      continue;

    uint8_t life_state = Memory::Read<uint8_t>(
        player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState);
    if (life_state != 0)
      continue;

    if (Settings::target_visible_check && !Game::IsVisible(player, local_index))
      continue;

    uintptr_t bone_matrix = Game::GetBoneMatrix(player);
    if (!bone_matrix)
      continue;

    Game::BoneEntry bones[30];
    if (!Game::ReadBoneArray(bone_matrix, bones, 30))
      continue;

    int bone_id = 6;
    if (cfg.bone == 1)
      bone_id = 5;
    else if (cfg.bone == 2)
      bone_id = 4;
    else if (cfg.bone == 3)
      bone_id = 2; // Stomach

    Vector3 target_pos = bones[bone_id].pos;

    if (target_pos.x == 0 && target_pos.y == 0)
      continue;

    Vector3 angle = CalculateAngle(eye_pos, target_pos);
    float fov = CalculateFov(eye_angles, angle);

    if (fov < best_fov) {
      best_fov = fov;
      best_angle = angle;
      best_target = player;
    }
  }

  if (best_target != 0) {
    if (Settings::target_smoothing && cfg.smooth > 1.0f) {
      Vector3 delta = best_angle - eye_angles;

      while (delta.y > 180)
        delta.y -= 360;
      while (delta.y < -180)
        delta.y += 360;

      best_angle = eye_angles + delta * (1.0f / cfg.smooth);
    }

    // Final normalization
    if (best_angle.x > 89.0f)
      best_angle.x = 89.0f;
    if (best_angle.x < -89.0f)
      best_angle.x = -89.0f;
    while (best_angle.y > 180)
      best_angle.y -= 360;
    while (best_angle.y < -180)
      best_angle.y += 360;
    best_angle.z = 0;

    if (GetAsyncKeyState(Settings::target_key_code)) {
      Game::SetViewAngles(best_angle);
    }
  }
}
