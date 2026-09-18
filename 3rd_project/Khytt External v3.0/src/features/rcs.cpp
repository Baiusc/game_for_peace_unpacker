#include "rcs.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>

Vector3 old_punch = {0, 0, 0};

void CombatMod::ApplyRCS() {
  if (!Settings::rcs_enabled) {
    old_punch = {0, 0, 0};
    return;
  }

  uintptr_t local_player = Game::GetLocalPlayerPawn();
  if (!local_player) {
    old_punch = {0, 0, 0};
    return;
  }

  int shots_fired = Memory::Read<int>(
      local_player +
      cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired);

  if (shots_fired > 1) {
    Vector3 punch = Memory::Read<Vector3>(
        local_player +
        cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle);
    Vector3 view_angles = Game::GetViewAngles();

    Vector3 new_angles = {view_angles.x + (old_punch.x - punch.x * 2.0f),
                          view_angles.y + (old_punch.y - punch.y * 2.0f), 0};

    // Normalize
    while (new_angles.y > 180)
      new_angles.y -= 360;
    while (new_angles.y < -180)
      new_angles.y += 360;

    if (new_angles.x > 89.0f)
      new_angles.x = 89.0f;
    if (new_angles.x < -89.0f)
      new_angles.x = -89.0f;

    Game::SetViewAngles(new_angles);

    old_punch = {punch.x * 2.0f, punch.y * 2.0f, 0};
  } else {
    old_punch = {0, 0, 0};
  }
}
