#pragma once
#include <Windows.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include "Downloader.h"

// Lightweight JSON-like parser for the dumper's specific output format
struct Offsets {
    // Globals (from offsets.json)
    uintptr_t dwEntityList = 0x24AC2A8;
    uintptr_t dwLocalPlayerCtrl = 0x22F1208;
    uintptr_t dwLocalPlayerPawn = 0x2066B60;
    uintptr_t dwViewMatrix = 0x230CFA0;
    uintptr_t dwViewAngles = 0x23176C8;
    uintptr_t dwPlantedC4 = 0x23147C0;
    uintptr_t dwGameRules = 0x230AF60;
    uintptr_t dwNetworkGameClient = 0x908500;
    uintptr_t dwGlobalVars = 0x205B5C0;

    // Schemas (client.dll)
    uintptr_t m_iHealth = 0x354;
    uintptr_t m_iTeamNum = 0x3F3;
    uintptr_t m_pGameSceneNode = 0x338;
    uintptr_t m_vecViewOffset = 0xD58;
    uintptr_t m_vecAbsVelocity = 0x404;
    uintptr_t m_modelState = 0x160;
    uintptr_t m_hPlayerPawn = 0x90C;
    uintptr_t m_iszPlayerName = 0x6F8;
    uintptr_t m_ArmorValue = 0x272C;
    uintptr_t m_flC4Blow = 0x11A0;
    uintptr_t m_bBombTicking = 0x1170;
    uintptr_t m_bBombDefused = 0x11C4;
    uintptr_t m_nBombSite = 0x1174;
    uintptr_t m_flTimerLength = 0x11A8;
    uintptr_t m_bBeingDefused = 0x11AC;
    uintptr_t m_flDefuseCountDown = 0x11C0;
    uintptr_t m_flDefuseLength = 0x11BC;
    uintptr_t m_steamID = 0x780;
    uintptr_t m_iIDEntIndex = 0x3EAC;
    uintptr_t m_flFlashDuration = 0x15F8;
    uintptr_t m_iShotsFired = 0x270C;
    uintptr_t m_aimPunchAngle = 0x16CC;
    uintptr_t m_pObserverServices = 0x13F0;
    uintptr_t m_hObserverTarget = 0x4C;

    // Skin Changer & Others
    uintptr_t m_pWeaponServices = 0x13D8;
    uintptr_t m_hMyWeapons = 0x48;
    uintptr_t m_hActiveWeapon = 0x60;
    uintptr_t m_flThrowVelocity = 0x778;
    uintptr_t m_bPinPulled = 0x1F43;
    uintptr_t m_fThrowTime = 0x1F48;
    uintptr_t m_flThrowStrength = 0x1F50;
    uintptr_t m_iItemDefinitionIndex = 0x1BA;
    uintptr_t m_entitySpottedState = 0x1F58; // updated build 14138
    uintptr_t m_bSpotted = 0x8;
    uintptr_t m_nFallbackPaintKit = 0x1850; // updated build 14138
    uintptr_t m_flFallbackWear = 0x1858;   // updated build 14138
    uintptr_t m_nFallbackStatTrak = 0x185C; // updated build 14138
    uintptr_t m_iItemIDHigh = 0x1D0;
    uintptr_t m_iAccountID = 0x1D8;
    uintptr_t m_OriginalOwnerXuidLow = 0x1848; // updated build 14138
    uintptr_t m_hOwnerEntity = 0x528;
    uintptr_t m_clrRender = 0xB80;
    uintptr_t m_nDeltaTick = 0x24C;
    uintptr_t m_Glow = 0xCC0;              // CGlowProperty, build 14138

    // Custom
    uintptr_t m_nodeAbsOrigin = 0xD0;
    uintptr_t m_boneArray = 0x80;

    bool UpdateFromWeb() {
        std::cout << "[*] Downloading offsets from GitHub..." << std::endl;
        std::string offsetsUrl = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json";
        std::string clientUrl  = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/client_dll.json";

        std::string offsetsJson = Downloader::DownloadString(offsetsUrl);
        if (offsetsJson.empty()) std::cout << "[!] Failed to download offsets.json" << std::endl;
        
        std::string clientJson  = Downloader::DownloadString(clientUrl);
        if (clientJson.empty()) std::cout << "[!] Failed to download client_dll.json" << std::endl;

        if (offsetsJson.empty() && clientJson.empty()) return false;

        if (!offsetsJson.empty()) ParseJSON(offsetsJson);
        if (!clientJson.empty())  ParseJSON(clientJson);

        return true;
    }

    bool LoadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        ParseJSON(content);
        return true;
    }

private:
    void ParseJSON(const std::string& json) {
        std::stringstream ss(json);
        std::string line;
        while (std::getline(ss, line)) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            size_t firstQuote = line.find('"');
            size_t secondQuote = line.find('"', firstQuote + 1);
            if (firstQuote == std::string::npos || secondQuote == std::string::npos) continue;

            std::string key = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            std::string valStr = line.substr(colon + 1);

            size_t firstDigit = valStr.find_first_of("0123456789");
            if (firstDigit == std::string::npos) continue;
            size_t lastDigit = valStr.find_last_of("0123456789");
            valStr = valStr.substr(firstDigit, lastDigit - firstDigit + 1);

            try {
                uintptr_t val = (uintptr_t)std::stoll(valStr);
                UpdateValue(key, val);
            } catch (...) {}
        }
    }

private:
    void UpdateValue(const std::string& key, uintptr_t val) {
        if (key == "dwEntityList") dwEntityList = val;
        else if (key == "dwLocalPlayerController" || key == "dwLocalPlayerCtrl") dwLocalPlayerCtrl = val;
        else if (key == "dwLocalPlayerPawn") dwLocalPlayerPawn = val;
        else if (key == "dwViewMatrix") dwViewMatrix = val;
        else if (key == "dwViewAngles") dwViewAngles = val;
        else if (key == "dwPlantedC4") dwPlantedC4 = val;
        else if (key == "dwGameRules") dwGameRules = val;
        else if (key == "dwNetworkGameClient") dwNetworkGameClient = val;
        else if (key == "dwGlobalVars") dwGlobalVars = val;
        else if (key == "m_iHealth") m_iHealth = val;
        else if (key == "m_iTeamNum") m_iTeamNum = val;
        else if (key == "m_pGameSceneNode") m_pGameSceneNode = val;
        else if (key == "m_vecViewOffset") m_vecViewOffset = val;
        else if (key == "m_modelState") m_modelState = val;
        else if (key == "m_hPlayerPawn") m_hPlayerPawn = val;
        else if (key == "m_iszPlayerName") m_iszPlayerName = val;
        else if (key == "m_ArmorValue") m_ArmorValue = val;
        else if (key == "m_flC4Blow") m_flC4Blow = val;
        else if (key == "m_bBombTicking") m_bBombTicking = val;
        else if (key == "m_bBombDefused") m_bBombDefused = val;
        else if (key == "m_nBombSite") m_nBombSite = val;
        else if (key == "m_flTimerLength") m_flTimerLength = val;
        else if (key == "m_bBeingDefused") m_bBeingDefused = val;
        else if (key == "m_flDefuseCountDown") m_flDefuseCountDown = val;
        else if (key == "m_flDefuseLength") m_flDefuseLength = val;
        else if (key == "m_steamID") m_steamID = val;
        else if (key == "m_iIDEntIndex") m_iIDEntIndex = val;
        else if (key == "m_flFlashDuration") m_flFlashDuration = val;
        else if (key == "m_iShotsFired") m_iShotsFired = val;
        else if (key == "m_aimPunchAngle") m_aimPunchAngle = val;
        else if (key == "m_pWeaponServices") m_pWeaponServices = val;
        else if (key == "m_hMyWeapons") m_hMyWeapons = val;
        else if (key == "m_hActiveWeapon") m_hActiveWeapon = val;
        else if (key == "m_iItemDefinitionIndex") m_iItemDefinitionIndex = val;
        else if (key == "m_nFallbackPaintKit") m_nFallbackPaintKit = val;
        else if (key == "m_flFallbackWear") m_flFallbackWear = val;
        else if (key == "m_nFallbackStatTrak") m_nFallbackStatTrak = val;
        else if (key == "m_iItemIDHigh") m_iItemIDHigh = val;
        else if (key == "m_iAccountID") m_iAccountID = val;
        else if (key == "m_OriginalOwnerXuidLow") m_OriginalOwnerXuidLow = val;
        else if (key == "m_hOwnerEntity") m_hOwnerEntity = val;
        else if (key == "m_clrRender") m_clrRender = val;
        else if (key == "m_nDeltaTick") m_nDeltaTick = val;
        else if (key == "m_pObserverServices") m_pObserverServices = val;
        else if (key == "m_hObserverTarget") m_hObserverTarget = val;
        else if (key == "m_flThrowVelocity") m_flThrowVelocity = val;
        else if (key == "m_bPinPulled") m_bPinPulled = val;
        else if (key == "m_fThrowTime") m_fThrowTime = val;
        else if (key == "m_flThrowStrength") m_flThrowStrength = val;
        else if (key == "m_entitySpottedState") m_entitySpottedState = val;
        else if (key == "m_bSpotted") m_bSpotted = val;
    }
};

extern Offsets g_Off;
