#include "bhop.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>


void Visuals::HandleBhop() {
  if (!Settings::auto_hop)
    return;

  uintptr_t local_player = Game::GetLocalPlayerPawn();
  if (!local_player)
    return;

  uint32_t flags = Memory::Read<uint32_t>(
      local_player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags);

  if (GetAsyncKeyState(VK_SPACE) &&
      (flags & (1 << 0))) { // 1 << 0 is FL_ONGROUND
    keybd_event(VK_SPACE, 0, 0, 0);
    keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
  }
}
