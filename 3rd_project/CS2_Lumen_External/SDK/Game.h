#pragma once
#include "../memory.h"
#include "../Offsets.h"
#include "Structs.h"
#include <iostream>
#include <string>

// Global pointers from Main.cpp
extern Memory* g_Memory;
extern uintptr_t g_Client;

namespace Game {
    inline uintptr_t GetEntityList() {
        if (!g_Client) return 0;
        return g_Memory->Read<uintptr_t>(g_Client + g_Off.dwEntityList);
    }

    inline uintptr_t GetEntityFromHandle(uint32_t handle, uintptr_t entityList) {
        if (!handle || !entityList)
            return 0;

        uintptr_t listEntry =
            g_Memory->Read<uintptr_t>(entityList + 0x10 + 8 * ((handle & 0x7FFF) >> 9));
        if (!listEntry)
            return 0;

        return g_Memory->Read<uintptr_t>(listEntry + 0x70 * (handle & 0x1FF));
    }

    inline uintptr_t GetEntityFromHandle(uint32_t handle) {
        return GetEntityFromHandle(handle, GetEntityList());
    }

    inline uintptr_t GetLocalPlayerController() {
        if (!g_Client) return 0;
        return g_Memory->Read<uintptr_t>(g_Client + g_Off.dwLocalPlayerCtrl);
    }

    inline uintptr_t GetLocalPlayerPawn() {
        uintptr_t controller = GetLocalPlayerController();
        if (!controller)
            return 0;

        uint32_t handle = g_Memory->Read<uint32_t>(controller + g_Off.m_hPlayerPawn);
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
            g_Memory->Read<uintptr_t>(entityList + 0x10 + 8 * (index >> 9));
        if (!listEntry)
            return 0;

        return g_Memory->Read<uintptr_t>(listEntry + 0x70 * (index & 0x1FF));
    }

    inline uintptr_t GetPlayerController(int index) {
        return GetPlayerController(GetEntityList(), index);
    }

    inline uintptr_t GetPawnFromController(uintptr_t controller, uintptr_t entityList) {
        if (!controller)
            return 0;

        uint32_t pawnHandle = g_Memory->Read<uint32_t>(controller + g_Off.m_hPlayerPawn);
        return GetEntityFromHandle(pawnHandle, entityList);
    }

    inline uintptr_t GetPawnFromController(uintptr_t controller) {
        return GetPawnFromController(controller, GetEntityList());
    }

    inline view_matrix_t GetViewMatrix() {
        if (!g_Client) return {};
        return g_Memory->Read<view_matrix_t>(g_Client + g_Off.dwViewMatrix);
    }

    inline GlobalVars GetGlobalVars() {
        if (!g_Client) return {};
        // Try reading it directly from the offset first
        return g_Memory->Read<GlobalVars>(g_Client + g_Off.dwGlobalVars);
    }

    inline uintptr_t GetBoneMatrix(uintptr_t pawn) {
        uintptr_t gameSceneNode = g_Memory->Read<uintptr_t>(pawn + g_Off.m_pGameSceneNode);
        if (!gameSceneNode)
            return 0;

        // m_modelState + m_boneArray
        return g_Memory->Read<uintptr_t>(gameSceneNode + g_Off.m_modelState + g_Off.m_boneArray);
    }

    inline uintptr_t GetActiveWeapon(uintptr_t pawn) {
        if (!pawn)
            return 0;

        uintptr_t weapon_services = g_Memory->Read<uintptr_t>(pawn + g_Off.m_pWeaponServices);
        if (!weapon_services)
            return 0;

        uint32_t weapon_handle = g_Memory->Read<uint32_t>(weapon_services + g_Off.m_hActiveWeapon);
        return GetEntityFromHandle(weapon_handle);
    }

    inline uint16_t GetItemDefinitionIndex(uintptr_t weapon) {
        if (!weapon)
            return 0;
        return g_Memory->Read<uint16_t>(weapon + g_Off.m_iItemDefinitionIndex);
    }

    inline Vector3 GetBonePos(uintptr_t boneMatrix, int boneIndex) {
        if (!boneMatrix)
            return {0, 0, 0};
        return g_Memory->Read<Vector3>(boneMatrix + boneIndex * 32);
    }

    inline std::string GetWeaponName(int weaponID) {
        switch (weaponID) {
        case 1:  return "Deagle";
        case 2:  return "Dual Berettas";
        case 3:  return "Five-SeveN";
        case 4:  return "Glock-18";
        case 7:  return "AK-47";
        case 8:  return "AUG";
        case 9:  return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 42: return "Knife";
        case 43: return "Flashbang";
        case 44: return "HE Grenade";
        case 45: return "Smoke Grenade";
        case 46: return "Molotov";
        case 47: return "Decoy";
        case 48: return "Incendiary";
        case 49: return "C4";
        case 61: return "USP-S";
        case 60: return "M4A1-S";
        case 63: return "CZ75-Auto";
        case 64: return "R8 Revolver";
        default: return "Weapon";
        }
    }

    inline int GetWeaponType(int weaponID) {
        switch (weaponID) {
        case 7: case 16: case 60: case 13: case 10: case 8: case 39: return 0; // Rifle
        case 17: case 34: case 23: case 24: case 33: case 19: case 26: return 1; // SMG
        case 1: case 2: case 3: case 4: case 30: case 32: case 36: case 61: case 63: case 64: return 2; // Pistol
        case 9: case 40: case 38: case 11: return 3; // Sniper
        default: return 0;
        }
    }
}
