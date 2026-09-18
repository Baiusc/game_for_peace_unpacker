#include "nospread.h"
#include "../sdk/config.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include <Windows.h>


void CombatMod::ApplyNoSpread() {
  if (!Settings::no_spread_toggle)
    return;

  // NoSpread in CS2 is typically done via weapon data modifications or
  // command manipulation. This is a placeholder for the toggle logic.
}
