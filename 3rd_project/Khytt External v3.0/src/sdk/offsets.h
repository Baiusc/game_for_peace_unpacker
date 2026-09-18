#pragma once
#include <cstdint>

namespace cs2_dumper {
namespace offsets {
namespace client_dll {
inline uintptr_t dwEntityList = 0x18C2D58;
inline uintptr_t dwLocalPlayerPawn = 0x17371A8;
inline uintptr_t dwLocalPlayerController = 0x1912578;
inline uintptr_t dwViewMatrix = 0x19241A0;
inline uintptr_t dwViewAngles = 0x19309E0;
inline uintptr_t dwGlobalVars = 0x172ABA0;
} // namespace client_dll
namespace engine2_dll {
inline uintptr_t dwNetworkGameClient = 0x9084E0;
} // namespace engine2_dll
} // namespace offsets

namespace schemas {
namespace client_dll {
namespace C_BaseEntity {
inline uintptr_t m_iTeamNum = 0x3BF;
inline uintptr_t m_iHealth = 0x334;
inline uintptr_t m_lifeState = 0x338;
inline uintptr_t m_pGameSceneNode = 0x318;
inline uintptr_t m_fFlags = 0x3D4;
} // namespace C_BaseEntity

namespace C_BasePlayerPawn {
inline uintptr_t m_vOldOrigin = 0x1274;
inline uintptr_t m_pWeaponServices = 0x1100;
inline uintptr_t m_pObserverServices = 0x1118;
} // namespace C_BasePlayerPawn

namespace C_CSPlayerPawn {
inline uintptr_t m_angEyeAngles = 0x1578;
inline uintptr_t m_aimPunchAngle = 0x177C;
inline uintptr_t m_iShotsFired = 0x147C;
inline uintptr_t m_iIDEntIndex = 0x13A8;
inline uintptr_t m_entitySpottedState = 0x2280;
} // namespace C_CSPlayerPawn

namespace C_BaseModelEntity {
inline uintptr_t m_vecViewOffset = 0xCB0;
} // namespace C_BaseModelEntity

namespace CCSPlayerController {
inline uintptr_t m_hPlayerPawn = 0x7E4;
inline uintptr_t m_sSanitizedPlayerName = 0x750;
} // namespace CCSPlayerController

namespace CPlayer_WeaponServices {
inline uintptr_t m_hActiveWeapon = 0x48;
} // namespace CPlayer_WeaponServices

namespace CPlayer_ObserverServices {
inline uintptr_t m_hObserverTarget = 0x44;
} // namespace CPlayer_ObserverServices

namespace C_PlantedC4 {
inline uintptr_t m_bBombTicking = 0xF51;
inline uintptr_t m_flC4Blow = 0xF54;
inline uintptr_t m_flTimerLength = 0xF58;
inline uintptr_t m_bBeingDefused = 0xF64;
inline uintptr_t m_flDefuseCountDown = 0xF68;
inline uintptr_t m_flDefuseLength = 0xF6C;
inline uintptr_t m_nBombSite = 0xF70;
} // namespace C_PlantedC4

} // namespace client_dll
} // namespace schemas
} // namespace cs2_dumper

namespace Offsets {
inline uintptr_t jump = 0x1730030;
} // namespace Offsets
