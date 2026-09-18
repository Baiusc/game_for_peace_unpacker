#include "fetcher.h"
#include "../features/bsp_parser.h"
#include "json.hpp"
#include "miniz.h"
#include "offsets.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <winhttp.h>


#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

std::string Fetcher::GetRawData(const std::string &url) {
  std::string result;
  HINTERNET hSession =
      WinHttpOpen(L"ExternalApp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return "";

  size_t pos = url.find("://");
  std::string host_full = url.substr(pos + 3);
  size_t slash_pos = host_full.find("/");
  std::string host = host_full.substr(0, slash_pos);
  std::string path = host_full.substr(slash_pos);

  std::wstring whost(host.begin(), host.end());
  std::wstring wpath(path.begin(), path.end());

  HINTERNET hConnect =
      WinHttpConnect(hSession, whost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return "";
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", wpath.c_str(), NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return "";
  }

  if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    if (WinHttpReceiveResponse(hRequest, NULL)) {
      DWORD dwStatusCode = 0;
      DWORD dwSize = sizeof(dwStatusCode);
      WinHttpQueryHeaders(hRequest,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                          WINHTTP_NO_HEADER_INDEX);

      if (dwStatusCode != 200) {
        std::cout << "[!] WinHTTP returned status code: " << dwStatusCode
                  << " for URL: " << url << std::endl;
      }

      dwSize = 0;
      do {
        if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
          std::vector<char> buffer(dwSize);
          DWORD dwDownloaded = 0;
          if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
            result.append(buffer.data(), dwDownloaded);
          }
        }
      } while (dwSize > 0);
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return result;
}

bool Fetcher::UpdateOffsets() {
  std::cout << "[info] synchronizing offsets..." << std::endl;

  std::string offsets_json =
      GetRawData("https://raw.githubusercontent.com/khytt1/"
                 "khytts-external-cs2/main/output/offsets.json");
  if (offsets_json.empty()) {
    std::cout << "[!] Failed to fetch offsets.json" << std::endl;
    return false;
  }

  std::string client_dll_json =
      GetRawData("https://raw.githubusercontent.com/khytt1/khytts-external-cs2/"
                 "main/output/"
                 "client_dll.json");
  if (client_dll_json.empty()) {
    std::cout << "[!] Failed to fetch client_dll.json" << std::endl;
    return false;
  }

  try {
    json j_offsets = json::parse(offsets_json);
    auto &client = j_offsets["client.dll"];

    using namespace cs2_dumper::offsets::client_dll;
    dwEntityList = client["dwEntityList"];
    dwLocalPlayerPawn = client["dwLocalPlayerPawn"];
    dwLocalPlayerController = client["dwLocalPlayerController"];
    dwViewMatrix = client["dwViewMatrix"];
    dwViewAngles = client["dwViewAngles"];
    dwGlobalVars = client["dwGlobalVars"];

    auto &engine2 = j_offsets["engine2.dll"];
    cs2_dumper::offsets::engine2_dll::dwNetworkGameClient =
        engine2["dwNetworkGameClient"];

    json j_client = json::parse(client_dll_json);
    auto &classes = j_client["client.dll"]["classes"];

    using namespace cs2_dumper::schemas::client_dll;
    C_BaseEntity::m_iTeamNum = classes["C_BaseEntity"]["fields"]["m_iTeamNum"];
    C_BaseEntity::m_iHealth = classes["C_BaseEntity"]["fields"]["m_iHealth"];
    C_BaseEntity::m_lifeState =
        classes["C_BaseEntity"]["fields"]["m_lifeState"];
    C_BaseEntity::m_pGameSceneNode =
        classes["C_BaseEntity"]["fields"]["m_pGameSceneNode"];
    C_BaseEntity::m_fFlags = classes["C_BaseEntity"]["fields"]["m_fFlags"];

    C_BasePlayerPawn::m_vOldOrigin =
        classes["C_BasePlayerPawn"]["fields"]["m_vOldOrigin"];
    C_BasePlayerPawn::m_pWeaponServices =
        classes["C_BasePlayerPawn"]["fields"]["m_pWeaponServices"];
    C_BasePlayerPawn::m_pObserverServices =
        classes["C_BasePlayerPawn"]["fields"]["m_pObserverServices"];

    C_CSPlayerPawn::m_angEyeAngles =
        classes["C_CSPlayerPawn"]["fields"]["m_angEyeAngles"];
    C_CSPlayerPawn::m_aimPunchAngle =
        classes["C_CSPlayerPawn"]["fields"]["m_aimPunchAngle"];
    C_CSPlayerPawn::m_iShotsFired =
        classes["C_CSPlayerPawn"]["fields"]["m_iShotsFired"];
    C_CSPlayerPawn::m_iIDEntIndex =
        classes["C_CSPlayerPawn"]["fields"]["m_iIDEntIndex"];
    C_CSPlayerPawn::m_entitySpottedState =
        classes["C_CSPlayerPawn"]["fields"]["m_entitySpottedState"];

    C_BaseModelEntity::m_vecViewOffset =
        classes["C_BaseModelEntity"]["fields"]["m_vecViewOffset"];

    CCSPlayerController::m_hPlayerPawn =
        classes["CCSPlayerController"]["fields"]["m_hPlayerPawn"];
    CCSPlayerController::m_sSanitizedPlayerName =
        classes["CCSPlayerController"]["fields"]["m_sSanitizedPlayerName"];

    CPlayer_WeaponServices::m_hActiveWeapon =
        classes["CPlayer_WeaponServices"]["fields"]["m_hActiveWeapon"];
    CPlayer_ObserverServices::m_hObserverTarget =
        classes["CPlayer_ObserverServices"]["fields"]["m_hObserverTarget"];

    C_PlantedC4::m_bBombTicking =
        classes["C_PlantedC4"]["fields"]["m_bBombTicking"];
    C_PlantedC4::m_flC4Blow = classes["C_PlantedC4"]["fields"]["m_flC4Blow"];
    C_PlantedC4::m_flTimerLength =
        classes["C_PlantedC4"]["fields"]["m_flTimerLength"];
    C_PlantedC4::m_bBeingDefused =
        classes["C_PlantedC4"]["fields"]["m_bBeingDefused"];
    C_PlantedC4::m_flDefuseCountDown =
        classes["C_PlantedC4"]["fields"]["m_flDefuseCountDown"];
    C_PlantedC4::m_flDefuseLength =
        classes["C_PlantedC4"]["fields"]["m_flDefuseLength"];
    C_PlantedC4::m_nBombSite = classes["C_PlantedC4"]["fields"]["m_nBombSite"];

    std::cout << "[done] offsets updated." << std::endl;
    return true;
  } catch (const json::exception &e) {
    std::cout << "[!] JSON parsing error: " << e.what() << std::endl;
    return false;
  }
}

bool Fetcher::DownloadMapArchive(const std::string &map_name) {
  std::filesystem::create_directory("maps");
  std::string file_path = "maps/" + map_name + ".vphys";

  // Check if map is already cached locally
  if (std::filesystem::exists(file_path)) {
    std::cout << "[info] Found local map cache: " << file_path << std::endl;
    std::ifstream file(file_path, std::ios::binary);
    if (file) {
      std::string vphys_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
      std::cout << "[info] Parsing cached map data for " << map_name << " ("
                << vphys_content.size() << " bytes)..." << std::endl;
      if (BspParser::LoadVPhysData(vphys_content))
        return true;
    }
    std::cout << "[!] Local cache corrupted / parser failed, falling back to "
                 "download..."
              << std::endl;
  }

  std::cout << "[info] Downloading map archive from GitHub..." << std::endl;
  std::string url = "https://github.com/khytt1/khytts-external-cs2/releases/"
                    "download/v1.0-maps/" +
                    map_name + ".zip";
  std::string zip_data = GetRawData(url);

  if (zip_data.empty()) {
    std::cout << "[!] Failed to download map archive: " << map_name << ".zip"
              << std::endl;
    return false;
  }

  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  if (!mz_zip_reader_init_mem(&zip_archive, zip_data.data(), zip_data.size(),
                              0)) {
    std::cout << "[!] Failed to initialize ZIP reader for: " << map_name
              << std::endl;
    return false;
  }

  bool success = false;
  int num_files = mz_zip_reader_get_num_files(&zip_archive);
  for (int i = 0; i < num_files; i++) {
    mz_zip_archive_file_stat file_stat;
    if (mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
      size_t uncomp_size = 0;
      void *p = mz_zip_reader_extract_file_to_heap(
          &zip_archive, file_stat.m_filename, &uncomp_size, 0);
      if (p) {
        std::string vphys_content((char *)p, uncomp_size);
        mz_free(p);

        // Cache to disk
        std::ofstream out_file(file_path, std::ios::binary);
        if (out_file) {
          out_file.write(vphys_content.data(), vphys_content.size());
          std::cout << "[info] Saved map cache to disk: " << file_path
                    << std::endl;
        }

        std::cout << "[info] Parsing map data for " << map_name << " ("
                  << uncomp_size << " bytes)..." << std::endl;
        success = BspParser::LoadVPhysData(vphys_content);
        break;
      }
    }
  }

  mz_zip_reader_end(&zip_archive);
  return success;
}
