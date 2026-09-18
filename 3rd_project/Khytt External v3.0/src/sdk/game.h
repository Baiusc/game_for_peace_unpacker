#pragma once
#include "memory.h"
#include "offsets.h"
#include "structs.h"
#include <iostream>

namespace Game {
inline uintptr_t GetEntityList() {
  return Memory::Read<uintptr_t>(Memory::ClientBase +
                                 cs2_dumper::offsets::client_dll::dwEntityList);
}

inline uintptr_t GetEntityFromHandle(uint32_t handle, uintptr_t entityList) {
  if (!handle || !entityList)
    return 0;

  uintptr_t listEntry =
      Memory::Read<uintptr_t>(entityList + 0x10 + 8 * ((handle & 0x7FFF) >> 9));
  if (!listEntry)
    return 0;

  return Memory::Read<uintptr_t>(listEntry + 0x70 * (handle & 0x1FF));
}

inline uintptr_t GetEntityFromHandle(uint32_t handle) {
  return GetEntityFromHandle(handle, GetEntityList());
}

inline uintptr_t GetLocalPlayerController() {
  return Memory::Read<uintptr_t>(
      Memory::ClientBase +
      cs2_dumper::offsets::client_dll::dwLocalPlayerController);
}

inline uintptr_t GetLocalPlayerPawn() {
  uintptr_t controller = GetLocalPlayerController();
  if (!controller)
    return 0;

  uint32_t handle = Memory::Read<uint32_t>(
      controller +
      cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
  return GetEntityFromHandle(handle);
}

struct BoneEntry {
  Vector3 pos;
  char pad[20];
};

inline uintptr_t GetPlayerController(uintptr_t entityList, int index) {
  if (!entityList)
    return 0;

  uintptr_t listEntry =
      Memory::Read<uintptr_t>(entityList + 0x10 + 8 * (index >> 9));
  if (!listEntry)
    return 0;

  return Memory::Read<uintptr_t>(listEntry + 0x70 * (index & 0x1FF));
}

inline uintptr_t GetPlayerController(int index) {
  return GetPlayerController(GetEntityList(), index);
}

inline uintptr_t GetPawnFromController(uintptr_t controller,
                                       uintptr_t entityList) {
  if (!controller)
    return 0;

  uint32_t pawnHandle = Memory::Read<uint32_t>(
      controller +
      cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
  return GetEntityFromHandle(pawnHandle, entityList);
}

inline uintptr_t GetPawnFromController(uintptr_t controller) {
  return GetPawnFromController(controller, GetEntityList());
}

inline view_matrix_t GetViewMatrix() {
  return Memory::Read<view_matrix_t>(
      Memory::ClientBase + cs2_dumper::offsets::client_dll::dwViewMatrix);
}

inline Vector3 GetViewAngles() {
  return Memory::Read<Vector3>(Memory::ClientBase +
                               cs2_dumper::offsets::client_dll::dwViewAngles);
}

inline void SetViewAngles(const Vector3 &angles) {
  Memory::Write<Vector3>(Memory::ClientBase +
                             cs2_dumper::offsets::client_dll::dwViewAngles,
                         angles);
}

inline GlobalVars GetGlobalVars() {
  return Memory::Read<GlobalVars>(
      Memory::ClientBase + cs2_dumper::offsets::client_dll::dwGlobalVars);
}

inline uintptr_t GetBoneMatrix(uintptr_t pawn) {
  uintptr_t sceneNode = Memory::Read<uintptr_t>(
      pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
  if (!sceneNode)
    return 0;

  return Memory::Read<uintptr_t>(sceneNode + 0x160 + 0x80);
}

inline bool IsVisible(uintptr_t target_pawn, int local_player_index) {
  if (!target_pawn || local_player_index <= 0)
    return false;

  // The spotted mask is an array of 2 uint32 (64 bits)
  // Each bit corresponds to a player index (1-64)
  int bit_index = local_player_index - 1;
  int array_idx = bit_index / 32;
  int mask_bit = bit_index % 32;

  uint32_t spotted_mask = Memory::Read<uint32_t>(
      target_pawn +
      cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_entitySpottedState +
      0xC + (array_idx * 4));

  return (spotted_mask & (1 << mask_bit)) != 0;
}

inline uintptr_t GetActiveWeapon(uintptr_t pawn) {
  if (!pawn)
    return 0;

  uintptr_t weapon_services = Memory::Read<uintptr_t>(pawn + 0x13D8);
  if (!weapon_services)
    return 0;

  uint32_t handle = Memory::Read<uint32_t>(weapon_services + 0x60);
  return GetEntityFromHandle(handle);
}

inline uint16_t GetItemDefinitionIndex(uintptr_t weapon) {
  if (!weapon)
    return 0;
  return Memory::Read<uint16_t>(weapon + 0x1582);
}

inline Vector3 GetBonePos(uintptr_t boneMatrix, int boneIndex) {
  if (!boneMatrix)
    return {0, 0, 0};
  // Each bone is a 32-byte transform matrix. Position is at the start (first 12
  // bytes).
  return Memory::Read<Vector3>(boneMatrix + boneIndex * 32);
}

// Batch version: Get bone position from a pre-read bone array
inline Vector3 GetBonePosCached(const BoneEntry *boneArray, int boneIndex) {
  return boneArray[boneIndex].pos;
}

inline bool ReadBoneArray(uintptr_t boneMatrix, BoneEntry *out, int count) {
  if (!boneMatrix)
    return false;
  return Memory::ReadBytes(boneMatrix, out, sizeof(BoneEntry) * count);
}

inline std::string GetWeaponName(int id) {
  switch (id) {
  case 1:
    return "Deagle";
  case 2:
    return "Dual Berettas";
  case 3:
    return "Five-SeveN";
  case 4:
    return "Glock-18";
  case 7:
    return "AK-47";
  case 8:
    return "AUG";
  case 9:
    return "AWP";
  case 10:
    return "FAMAS";
  case 11:
    return "G3SG1";
  case 13:
    return "Galil AR";
  case 14:
    return "M249";
  case 16:
    return "M4A4";
  case 17:
    return "MAC-10";
  case 19:
    return "P90";
  case 23:
    return "MP5-SD";
  case 24:
    return "UMP-45";
  case 25:
    return "XM1014";
  case 26:
    return "Bizon";
  case 27:
    return "MAG-7";
  case 28:
    return "Negev";
  case 29:
    return "Sawed-Off";
  case 30:
    return "Tec-9";
  case 31:
    return "Zeus";
  case 32:
    return "P2000";
  case 33:
    return "MP7";
  case 34:
    return "MP9";
  case 35:
    return "Nova";
  case 36:
    return "P250";
  case 38:
    return "SCAR-20";
  case 39:
    return "SG 553";
  case 40:
    return "SSG 08";
  case 42:
    return "Knife";
  case 43:
    return "Flashbang";
  case 44:
    return "HE Grenade";
  case 45:
    return "Smoke Grenade";
  case 46:
    return "Molotov";
  case 47:
    return "Decoy";
  case 48:
    return "Incendiary";
  case 49:
    return "C4";
  case 61:
    return "USP-S";
  case 60:
    return "M4A1-S";
  case 63:
    return "CZ75-Auto";
  case 64:
    return "R8 Revolver";
  default:
    return "Weapon";
  }
}

inline int GetWeaponType(int id) {
  switch (id) {
  case 7:
  case 16:
  case 60:
  case 13:
  case 10:
  case 8:
  case 39:
    return 0; // Rifles
  case 1:
  case 2:
  case 3:
  case 4:
  case 30:
  case 32:
  case 36:
  case 61:
  case 63:
  case 64:
    return 1; // Pistols
  case 9:
  case 40:
  case 38:
  case 11:
    return 2; // Snipers
  case 17:
  case 34:
  case 23:
  case 24:
  case 33:
  case 19:
  case 26:
    return 3; // SMGs
  default:
    return 0;
  }
}
} // namespace Game
