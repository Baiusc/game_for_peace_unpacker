#include "esp.h"
#include "../imgui/imgui.h"
#include "../overlay.h"
#include "../sdk/config.h"
#include "../sdk/fetcher.h"
#include "../sdk/game.h"
#include "../sdk/memory.h"
#include "../sdk/offsets.h"
#include "bsp_parser.h"
#include <Windows.h>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct CachedESP {
  Vector3 origin;
  Vector3 head_pos;
  int health;
  int team;
  bool is_visible;
  std::string name;
  std::string weapon;
  Vector3 bones[30];
  bool skeleton_valid;
  bool wallbangable;
};

std::vector<CachedESP> front_buffer;
std::mutex data_mutex;

bool Visuals::Initialize() {
  front_buffer.reserve(64);
  return true;
}

void Visuals::CalculateESP() {
  while (!Settings::request_exit) {
    if (!Settings::visual_esp && !Settings::target_enabled) {
      {
        std::lock_guard<std::mutex> lock(data_mutex);
        front_buffer.clear();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    uintptr_t local_player = Game::GetLocalPlayerPawn();
    if (!local_player) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // Cache common data once per cycle
    uintptr_t client_dll = Memory::ClientBase;
    uintptr_t global_vars = Memory::Read<uintptr_t>(
        client_dll + cs2_dumper::offsets::client_dll::dwGlobalVars);

    if (global_vars) {
      uintptr_t map_name_ptr = Memory::Read<uintptr_t>(global_vars + 0x188);
      std::string current_map =
          map_name_ptr ? Memory::ReadString(map_name_ptr, 64) : "";

      static std::string last_map = "";
      if (!current_map.empty() && current_map != last_map) {
        last_map = current_map;
        std::string parse_map = current_map;
        size_t slash = parse_map.find_last_of("\\/");
        if (slash != std::string::npos)
          parse_map = parse_map.substr(slash + 1);

        size_t dot = parse_map.find_last_of(".");
        if (dot != std::string::npos)
          parse_map = parse_map.substr(0, dot);

        // Filter out junk memory reads (must be valid printable characters like
        // 'de_dust2')
        bool is_valid_name = true;
        for (char c : parse_map) {
          if (!isalnum(c) && c != '_' && c != '-') {
            is_valid_name = false;
            break;
          }
        }

        if (is_valid_name && parse_map != "<empty>" && !parse_map.empty()) {
          BspParser::is_map_loaded = false; // Reset map collision
          snprintf(BspParser::map_name_loaded,
                   sizeof(BspParser::map_name_loaded), "Fetching %s.zip...",
                   parse_map.c_str());
          // Detach the map fetcher into a background thread so the ESP never
          // freezes!
          std::thread([parse_map]() {
            if (Fetcher::DownloadMapArchive(parse_map)) {
              snprintf(BspParser::map_name_loaded,
                       sizeof(BspParser::map_name_loaded), "Map Loaded: %s",
                       parse_map.c_str());
            } else {
              snprintf(BspParser::map_name_loaded,
                       sizeof(BspParser::map_name_loaded), "Failed to load %s",
                       parse_map.c_str());
            }
          }).detach();
        }
      }
    }

    uintptr_t entity_list = Game::GetEntityList();
    int local_team = Memory::Read<int>(
        local_player +
        cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    view_matrix_t current_matrix = Game::GetViewMatrix();

    uintptr_t local_controller = Game::GetLocalPlayerController();
    int local_index = -1;
    for (int i = 0; i <= 64; i++) {
      if (Game::GetPlayerController(entity_list, i) == local_controller) {
        local_index = i;
        break;
      }
    }
    if (local_index == -1)
      local_index = 0;

    std::vector<CachedESP> back_buffer;

    for (int i = 1; i <= 64; i++) {
      uintptr_t controller = Game::GetPlayerController(entity_list, i);
      if (!controller)
        continue;

      uintptr_t player = Game::GetPawnFromController(controller, entity_list);
      if (!player || player == local_player)
        continue;

      // Batch read health and team (contiguous)
      struct EntityData {
        int health;
        int team;
      } data;

      // non-contiguous, individual reads
      data.health = Memory::Read<int>(
          player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
      if (data.health <= 0 || data.health > 100)
        continue;

      data.team = Memory::Read<int>(
          player + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
      if (!Settings::visual_team_check && data.team == local_team)
        continue;

      Vector3 origin = Memory::Read<Vector3>(
          player +
          cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);

      uintptr_t bone_matrix = Game::GetBoneMatrix(player);
      if (!bone_matrix)
        continue;

      // batch read bones
      Game::BoneEntry bones[30];
      if (!Game::ReadBoneArray(bone_matrix, bones, 30))
        continue;

      Vector3 head_pos = bones[6].pos;
      head_pos.z += 8.0f;

      bool skeleton_valid = false;
      if (Settings::visual_skeleton) {
        skeleton_valid = true;
      }

      std::string name = "Player";
      if (Settings::visual_names) {
        uintptr_t name_ptr = Memory::Read<uintptr_t>(
            controller + cs2_dumper::schemas::client_dll::CCSPlayerController::
                             m_sSanitizedPlayerName);
        if (name_ptr) {
          name = Memory::ReadString(name_ptr,
                                    32); // Max name length typically 32
        }
      }

      std::string weapon_name = "";
      if (Settings::visual_weapon) {
        uintptr_t active_weapon = Game::GetActiveWeapon(player);
        if (active_weapon) {
          uint16_t item_idx = Game::GetItemDefinitionIndex(active_weapon);
          weapon_name = Game::GetWeaponName(item_idx);
        }
      }

      CachedESP esp_inst;
      esp_inst.wallbangable = false;

      bool is_visible = Game::IsVisible(player, local_index);

      if (!is_visible && BspParser::is_map_loaded) {
        Vector3 head_pos_v = Game::GetBonePos(Game::GetBoneMatrix(player), 6);
        Vector3 local_eye = Memory::Read<Vector3>(
            local_player +
            cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 view_offset = Memory::Read<Vector3>(
            local_player + cs2_dumper::schemas::client_dll::C_BaseModelEntity::
                               m_vecViewOffset);
        local_eye.x += view_offset.x;
        local_eye.y += view_offset.y;
        local_eye.z += view_offset.z;
        is_visible = BspParser::IsVisible(local_eye, head_pos_v);

        if (!is_visible) {
          float penetration_depth =
              BspParser::GetPenetrationDepth(local_eye, head_pos_v);
          // CS2 general penetration limit for most rifles is ~32-50 units
          // through thin wood/walls.
          esp_inst.wallbangable =
              (penetration_depth > 0.0f && penetration_depth < 35.0f);
        }
      }

      esp_inst.origin = origin;
      esp_inst.head_pos = head_pos;
      esp_inst.health = data.health;
      esp_inst.team = data.team;
      esp_inst.is_visible = is_visible;
      esp_inst.name = name;
      esp_inst.weapon = weapon_name;
      esp_inst.skeleton_valid = skeleton_valid;

      if (skeleton_valid) {
        for (int b = 0; b < 30; ++b) {
          esp_inst.bones[b] = bones[b].pos;
        }
      }

      back_buffer.push_back(esp_inst);
    }

    {
      std::lock_guard<std::mutex> lock(data_mutex);
      front_buffer = back_buffer;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void Visuals::DrawOverlay() {
  if (!Settings::visual_esp)
    return;

  std::vector<CachedESP> draw_list_local;
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    draw_list_local = front_buffer;
  }

  ImDrawList *draw_list = ImGui::GetForegroundDrawList();

  // Zero-Delay Engine: Sync view matrix exactly when matching rendering frames
  uintptr_t client_dll = Memory::ClientBase;
  view_matrix_t render_matrix = Memory::Read<view_matrix_t>(
      client_dll + cs2_dumper::offsets::client_dll::dwViewMatrix);

  // Draw Map Status Indicator Top-Left
  draw_list->AddText(ImVec2(20, 150), IM_COL32(0, 255, 0, 255),
                     BspParser::map_name_loaded);

  for (const auto &esp : draw_list_local) {
    Vector2 screen_pos, screen_head;

    // Perform spatial screen projection dynamically
    if (!WorldToScreen(esp.origin, screen_pos, render_matrix, 1920, 1080) ||
        !WorldToScreen(esp.head_pos, screen_head, render_matrix, 1920, 1080)) {
      continue;
    }

    float height = screen_pos.y - screen_head.y;
    float width = height / 2.0f;

    ImVec2 top_left(screen_head.x - width / 2.0f, screen_head.y);
    ImVec2 bottom_right(screen_pos.x + width / 2.0f, screen_pos.y);

    if (Settings::visual_box) {
      ImU32 box_col;

      if (esp.is_visible) {
        box_col = IM_COL32((int)(Settings::visual_box_color[0] * 255),
                           (int)(Settings::visual_box_color[1] * 255),
                           (int)(Settings::visual_box_color[2] * 255), 255);
      } else {
        box_col =
            IM_COL32((int)(Settings::visual_box_color_hidden[0] * 255),
                     (int)(Settings::visual_box_color_hidden[1] * 255),
                     (int)(Settings::visual_box_color_hidden[2] * 255), 255);
      }

      // 1. Box Background Fill
      draw_list->AddRectFilled(top_left, bottom_right, IM_COL32(0, 0, 0, 60),
                               4.0f);

      // 2. Black Box Outline (outer/inner layers)
      draw_list->AddRect(ImVec2(top_left.x - 1, top_left.y - 1),
                         ImVec2(bottom_right.x + 1, bottom_right.y + 1),
                         IM_COL32(0, 0, 0, 200), 4.0f);
      draw_list->AddRect(ImVec2(top_left.x + 1, top_left.y + 1),
                         ImVec2(bottom_right.x - 1, bottom_right.y - 1),
                         IM_COL32(0, 0, 0, 200), 4.0f);

      // 3. Colored Main Box
      draw_list->AddRect(top_left, bottom_right, box_col, 4.0f);
    }

    if (Settings::visual_hp_bar) {
      float hp_height = height * (esp.health / 100.0f);

      // Dynamic Health Color Shift
      ImU32 hp_color = esp.health > 50   ? IM_COL32(0, 255, 0, 255)
                       : esp.health > 20 ? IM_COL32(255, 200, 0, 255)
                                         : IM_COL32(255, 0, 0, 255);

      // Background Track
      draw_list->AddRectFilled(ImVec2(top_left.x - 6, bottom_right.y - height),
                               ImVec2(top_left.x - 2, bottom_right.y),
                               IM_COL32(0, 0, 0, 150));

      // Outline
      draw_list->AddRect(ImVec2(top_left.x - 7, bottom_right.y - height - 1),
                         ImVec2(top_left.x - 1, bottom_right.y + 1),
                         IM_COL32(0, 0, 0, 200));

      // Foreground HP
      draw_list->AddRectFilled(
          ImVec2(top_left.x - 5, bottom_right.y - hp_height),
          ImVec2(top_left.x - 3, bottom_right.y), hp_color);
    }

    if (Settings::visual_names) {
      ImVec2 text_size = ImGui::CalcTextSize(esp.name.c_str());
      ImVec2 text_pos(screen_head.x - text_size.x / 2.0f,
                      screen_head.y - text_size.y - 4);

      // Text Shadow/Outline
      draw_list->AddText(ImVec2(text_pos.x + 1, text_pos.y + 1),
                         IM_COL32(0, 0, 0, 255), esp.name.c_str());
      // Text Foreground
      draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255),
                         esp.name.c_str());
    }

    if (Settings::visual_weapon && !esp.weapon.empty()) {
      ImVec2 w_text_size = ImGui::CalcTextSize(esp.weapon.c_str());
      ImVec2 w_text_pos(screen_pos.x - w_text_size.x / 2.0f, screen_pos.y + 2);

      // Text Shadow/Outline
      draw_list->AddText(ImVec2(w_text_pos.x + 1, w_text_pos.y + 1),
                         IM_COL32(0, 0, 0, 255), esp.weapon.c_str());
      // Text Foreground
      draw_list->AddText(w_text_pos, IM_COL32(200, 200, 200, 255),
                         esp.weapon.c_str());
    }

    if (esp.wallbangable) {
      ImVec2 wb_pos(top_left.x + width + 5, top_left.y);
      draw_list->AddText(ImVec2(wb_pos.x + 1, wb_pos.y + 1),
                         IM_COL32(0, 0, 0, 255), "WALLBANG");
      draw_list->AddText(wb_pos, IM_COL32(255, 165, 0, 255), "WALLBANG");
    }

    if (Settings::visual_tracers) {
      ImU32 tracer_col =
          IM_COL32((int)(Settings::visual_tracers_color[0] * 255),
                   (int)(Settings::visual_tracers_color[1] * 255),
                   (int)(Settings::visual_tracers_color[2] * 255), 255);
      ImVec2 screen_bottom(1920 / 2.0f, 1080);
      draw_list->AddLine(screen_bottom, ImVec2(screen_pos.x, screen_pos.y),
                         tracer_col, 1.5f);
    }

    if (Settings::visual_skeleton && esp.skeleton_valid) {
      ImU32 skel_col;
      if (esp.is_visible) {
        skel_col =
            IM_COL32((int)(Settings::visual_skeleton_color[0] * 255),
                     (int)(Settings::visual_skeleton_color[1] * 255),
                     (int)(Settings::visual_skeleton_color[2] * 255), 255);
      } else {
        skel_col = IM_COL32(
            (int)(Settings::visual_skeleton_color_hidden[0] * 255),
            (int)(Settings::visual_skeleton_color_hidden[1] * 255),
            (int)(Settings::visual_skeleton_color_hidden[2] * 255), 255);
      }

      Vector2 bones_screen[30];
      bool skeleton_on_screen = true;
      for (int b = 0; b < 30; ++b) {
        if (!WorldToScreen(esp.bones[b], bones_screen[b], render_matrix, 1920,
                           1080)) {
          skeleton_on_screen = false;
          break;
        }
      }

      if (skeleton_on_screen) {
        auto draw_bone_line = [&](int b1, int b2) {
          draw_list->AddLine(ImVec2(bones_screen[b1].x, bones_screen[b1].y),
                             ImVec2(bones_screen[b2].x, bones_screen[b2].y),
                             skel_col, 1.0f);
        };

        // Spine/Core
        draw_bone_line(6, 5);
        draw_bone_line(5, 4);
        draw_bone_line(4, 0); // Pelvis approx

        // Left Arm
        draw_bone_line(5, 8);
        draw_bone_line(8, 9);
        draw_bone_line(9, 10);

        // Right Arm
        draw_bone_line(5, 13);
        draw_bone_line(13, 14);
        draw_bone_line(14, 15);

        // Left Leg
        draw_bone_line(0, 22);
        draw_bone_line(22, 23);
        draw_bone_line(23, 24);

        // Right Leg
        draw_bone_line(0, 25);
        draw_bone_line(25, 26);
        draw_bone_line(26, 27);
      }
    }
  }

  DrawGrenadeTrajectory();
  DrawFloatingMascot();
}

void Visuals::DrawFloatingMascot() {
  ;
  DrawCrosshair();
  DrawWatermark();
  DrawSafeModeIndicator();
  DrawInjectionNotification();
}

static float notification_alpha = 0.0f;
static float notification_start_time = -1.0f;
static bool notification_active = false;

void Visuals::TriggerNotification() {
  notification_active = true;
  notification_start_time = -1.0f; // Reset to signal "needs start time"
  notification_alpha = 0.0f;
}

void Visuals::DrawInjectionNotification() {
  if (!notification_active)
    return;

  if (notification_start_time < 0.0f)
    notification_start_time = (float)ImGui::GetTime();

  float current_time = (float)ImGui::GetTime();
  float elapsed = current_time - notification_start_time;
  const float duration = 4.0f;  // 4 seconds total
  const float fade_time = 0.8f; // 0.8s fade in/out

  if (elapsed > duration) {
    notification_active = false;
    return;
  }

  if (elapsed < fade_time) {
    notification_alpha = elapsed / fade_time;
  } else if (elapsed > (duration - fade_time)) {
    notification_alpha = (duration - elapsed) / fade_time;
  } else {
    notification_alpha = 1.0f;
  }

  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
  ImVec2 center(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f);

  // --- Aero Dynamic Enhancements (v3.7) ---
  // 1. Time-based displacement (Floating/Bobbing)
  float time = (float)ImGui::GetTime();
  float bobbing_y = sin(time * 3.0f) * 15.0f; // Smooth vertical float

  // 2. Silhouette Animation (Scale + Entrance)
  float reveal_duration = 1.0f;
  float entrance_scale = 1.0f;
  if (elapsed < reveal_duration) {
    entrance_scale =
        0.8f + (elapsed / reveal_duration) * 0.2f; // Scale 0.8 -> 1.0
  }

  // 3. Pulse / Glow (Breathing effect)
  float pulse = (sin(time * 4.0f) + 1.0f) / 2.0f; // 0.0 -> 1.0
  int glow_alpha = (int)(notification_alpha * (100 + (pulse * 80)));

  // Menu Purple Tokens: 0.60f, 0.40f, 1.00f
  ImU32 white_col = IM_COL32(255, 255, 255, (int)(notification_alpha * 255));
  ImU32 purple_glow = IM_COL32(153, 102, 255, glow_alpha);
  ImU32 shadow_col = IM_COL32(0, 0, 0, (int)(notification_alpha * 180));
  ImU32 accent_col = IM_COL32(
      100, 50, 200, (int)(notification_alpha * 120)); // Deeper purple accent

  // --- Render Notification Elements ---

  // 1. Soldier Silhouette (Texture)
  if (Overlay::SilhouetteTexture) {
    float tex_w = (float)Overlay::SilhouetteWidth;
    float tex_h = (float)Overlay::SilhouetteHeight;
    float base_h = 240.0f;
    float target_h = base_h * entrance_scale; // Dynamic scale
    float target_w = tex_w * (target_h / tex_h);

    // Apply bobbing to vertical position
    ImVec2 tex_pos(center.x - target_w / 2.0f, center.y - 260.0f + bobbing_y);

    // Pulse Purple Glow Background
    draw_list->AddImage(
        Overlay::SilhouetteTexture, ImVec2(tex_pos.x - 2, tex_pos.y - 2),
        ImVec2(tex_pos.x + target_w + 2, tex_pos.y + target_h + 2),
        ImVec2(0, 0), ImVec2(1, 1), purple_glow);

    draw_list->AddImage(Overlay::SilhouetteTexture, tex_pos,
                        ImVec2(tex_pos.x + target_w, tex_pos.y + target_h),
                        ImVec2(0, 0), ImVec2(1, 1), white_col);
  }

  // 2. Main Banner (64px)
  ImGui::PushFont(io.Fonts->Fonts[2]);
  std::string main_text = "fly like a khytt";
  ImVec2 text_size = ImGui::CalcTextSize(main_text.c_str());

  // Positioned below image with bobbing
  ImVec2 pos(center.x - text_size.x / 2.0f,
             center.y + 0.0f + bobbing_y); // Applied vertical float

  // Layered shadow for depth
  draw_list->AddText(ImVec2(pos.x + 3, pos.y + 3), shadow_col,
                     main_text.c_str());

  // Chromatic Purple Accent (Shadow Layer)
  draw_list->AddText(ImVec2(pos.x + 2, pos.y + 2), accent_col,
                     main_text.c_str());

  // Outer Pulse Glow for text (Purple)
  draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), purple_glow,
                     main_text.c_str());
  draw_list->AddText(ImVec2(pos.x - 1, pos.y - 1), purple_glow,
                     main_text.c_str());

  draw_list->AddText(pos, white_col, main_text.c_str());
  ImGui::PopFont(); // Pop 64px

  // 3. Subtext (18px)
  ImGui::PushFont(io.Fonts->Fonts[1]);
  std::string subtext = "ʚ   ɞ";
  ImVec2 sub_size = ImGui::CalcTextSize(subtext.c_str());
  ImVec2 sub_pos(center.x - sub_size.x / 2.0f, pos.y + text_size.y + 5.0f);

  // Shadow for subtext (Subtle Purple)
  draw_list->AddText(ImVec2(sub_pos.x + 1, sub_pos.y + 1), accent_col,
                     subtext.c_str());
  draw_list->AddText(sub_pos, white_col, subtext.c_str());
  ImGui::PopFont(); // Pop 18px
}

void Visuals::DrawCrosshair() {
  if (!Settings::visual_crosshair)
    return;

  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

  ImVec2 center(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f);
  float size = Settings::visual_crosshair_size;

  ImU32 col = IM_COL32((int)(Settings::visual_crosshair_color[0] * 255),
                       (int)(Settings::visual_crosshair_color[1] * 255),
                       (int)(Settings::visual_crosshair_color[2] * 255), 255);

  draw_list->AddLine(ImVec2(center.x - size, center.y),
                     ImVec2(center.x + size, center.y), col, 1.0f);
  draw_list->AddLine(ImVec2(center.x, center.y - size),
                     ImVec2(center.x, center.y + size), col, 1.0f);
}

void Visuals::DrawFOVCircle() {
  if (!Settings::draw_fov_circle)
    return;

  auto &cfg = Settings::weapon_configs[Settings::current_weapon_type];
  if (cfg.fov <= 0.0f)
    return;

  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

  float screen_w = io.DisplaySize.x;
  float screen_h = io.DisplaySize.y;

  // Simple FOV to Pixel conversion (approximate for external)
  // FOV in degrees, we want radius in pixels.
  // Radius = (fov / horizontal_fov_of_game) * screen_width / 2
  // Assuming default CS2 horizontal FOV is ~90-106 depending on aspect ratio.
  float radius = (cfg.fov / 90.0f) * (screen_w / 2.0f);

  draw_list->AddCircle(ImVec2(screen_w / 2.0f, screen_h / 2.0f), radius,
                       IM_COL32(200, 200, 200, 150), 64, 1.0f);
}

void Visuals::DrawGrenadeTrajectory() {
  if (!BspParser::is_map_loaded)
    return;

  uintptr_t local_player = Game::GetLocalPlayerPawn();
  if (!local_player)
    return;

  uintptr_t active_weapon = Game::GetActiveWeapon(local_player);
  if (!active_weapon)
    return;

  uint16_t item_idx = Game::GetItemDefinitionIndex(active_weapon);
  std::string wep_name = Game::GetWeaponName(item_idx);

  // Simple check for if we are holding a grenade
  if (wep_name != "Flashbang" && wep_name != "HE Grenade" &&
      wep_name != "Smoke Grenade" && wep_name != "Molotov" &&
      wep_name != "Decoy" && wep_name != "Incendiary") {
    return;
  }

  // Get launch origin (Eye position)
  Vector3 eye_pos = Memory::Read<Vector3>(
      local_player +
      cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
  Vector3 view_offset = Memory::Read<Vector3>(
      local_player +
      cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
  eye_pos.x += view_offset.x;
  eye_pos.y += view_offset.y;
  eye_pos.z += view_offset.z;

  // Read view angles (Pitch, Yaw)
  Vector2 view_angles = Memory::Read<Vector2>(
      local_player +
      cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles);

  // Convert angles to forward vector
  float pitch_rad = view_angles.x * (3.14159f / 180.0f);
  float yaw_rad = view_angles.y * (3.14159f / 180.0f);

  Vector3 forward = {std::cos(yaw_rad) * std::cos(pitch_rad),
                     std::sin(yaw_rad) * std::cos(pitch_rad),
                     -std::sin(pitch_rad)};

  // Initial velocity (Approximate throw power + throw angle offsets)
  float throw_velocity = 900.0f;
  Vector3 velocity = {forward.x * throw_velocity, forward.y * throw_velocity,
                      forward.z * throw_velocity};

  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

  // Need the active render matrix to draw the 3D lines
  uintptr_t client_dll = Memory::ClientBase;
  view_matrix_t render_matrix = Memory::Read<view_matrix_t>(
      client_dll + cs2_dumper::offsets::client_dll::dwViewMatrix);

  Vector3 current_pos = eye_pos;
  float time_step = 0.05f; // Resolution of simulation
  float max_time = 2.0f;   // Predict 2 seconds into future
  float gravity = -800.0f;

  std::vector<ImVec2> screen_points;

  for (float t = 0.0f; t < max_time; t += time_step) {
    Vector3 next_pos = current_pos;
    next_pos.x += velocity.x * time_step;
    next_pos.y += velocity.y * time_step;
    next_pos.z += velocity.z * time_step;

    // Apply gravity to velocity for NEXT tick
    velocity.z += gravity * time_step;

    // Check collision between current_pos and next_pos
    Vector3 ray_dir = {next_pos.x - current_pos.x, next_pos.y - current_pos.y,
                       next_pos.z - current_pos.z};

    float ray_len = std::sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y +
                              ray_dir.z * ray_dir.z);
    if (ray_len > 0.0f) {
      ray_dir.x /= ray_len;
      ray_dir.y /= ray_len;
      ray_dir.z /= ray_len;

      float min_t = ray_len;
      BspParser::Triangle hit_tri;
      bool did_hit = false;

      // Custom fast raycast directly inside the esp drawing loop
      for (const auto &tri : BspParser::map_triangles) {
        float hit_t = 0.0f;
        if (BspParser::IntersectRayTriangle(current_pos, ray_dir, hit_t, tri)) {
          if (hit_t > 0.0f && hit_t < min_t) {
            min_t = hit_t;
            hit_tri = tri;
            did_hit = true;
          }
        }
      }

      if (did_hit) {
        // We hit a wall. Move position EXACTLY to the wall.
        next_pos.x = current_pos.x + ray_dir.x * min_t;
        next_pos.y = current_pos.y + ray_dir.y * min_t;
        next_pos.z = current_pos.z + ray_dir.z * min_t;

        // Calculate normal of triangle
        Vector3 edge1 = {hit_tri.v1.x - hit_tri.v0.x,
                         hit_tri.v1.y - hit_tri.v0.y,
                         hit_tri.v1.z - hit_tri.v0.z};
        Vector3 edge2 = {hit_tri.v2.x - hit_tri.v0.x,
                         hit_tri.v2.y - hit_tri.v0.y,
                         hit_tri.v2.z - hit_tri.v0.z};

        Vector3 normal = {edge1.y * edge2.z - edge1.z * edge2.y,
                          edge1.z * edge2.x - edge1.x * edge2.z,
                          edge1.x * edge2.y - edge1.y * edge2.x};

        float normal_len = std::sqrt(normal.x * normal.x + normal.y * normal.y +
                                     normal.z * normal.z);
        if (normal_len > 0.0f) {
          normal.x /= normal_len;
          normal.y /= normal_len;
          normal.z /= normal_len;
        }

        // Reflect velocity vector
        float dotProduct = velocity.x * normal.x + velocity.y * normal.y +
                           velocity.z * normal.z;
        float elasticity = 0.45f; // Grenades lose speed on bounce

        velocity.x = (velocity.x - 2.0f * dotProduct * normal.x) * elasticity;
        velocity.y = (velocity.y - 2.0f * dotProduct * normal.y) * elasticity;
        velocity.z = (velocity.z - 2.0f * dotProduct * normal.z) * elasticity;
      }
    }

    Vector2 screen_p;
    if (WorldToScreen(current_pos, screen_p, render_matrix, 1920, 1080)) {
      screen_points.push_back(ImVec2(screen_p.x, screen_p.y));
    }

    current_pos = next_pos;
  }

  // Draw the full trajectory arc
  if (screen_points.size() > 1) {
    draw_list->AddPolyline(screen_points.data(), screen_points.size(),
                           IM_COL32(255, 255, 255, 200), ImDrawFlags_None,
                           2.0f);

    // Draw end box where grenade lands
    ImVec2 end_p = screen_points.back();
    draw_list->AddRect(ImVec2(end_p.x - 4, end_p.y - 4),
                       ImVec2(end_p.x + 4, end_p.y + 4),
                       IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
  }
}

void Visuals::DrawWatermark() {
  if (!Settings::draw_watermark)
    return;

  ImGui::SetNextWindowPos(ImVec2(10, 10));
  ImGui::Begin("Watermark", NULL,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

  ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "External v2.8");
  ImGui::End();
}

void Visuals::DrawSafeModeIndicator() {
  // Safe mode usually refers to the safety_lock which might prevent some RAGE
  // features
  ImGui::SetNextWindowPos(ImVec2(10, 35));
  ImGui::Begin("SafeMode", NULL,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

  if (Settings::safety_lock) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SAFE MODE: ON");
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                       "SAFE MODE: OFF (UNSAFE)");
  }
  ImGui::End();
}

void Visuals::Cleanup() {
  std::lock_guard<std::mutex> lock(data_mutex);
  front_buffer.clear();
}
