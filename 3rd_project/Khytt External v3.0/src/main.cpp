#define IMGUI_DEFINE_MATH_OPERATORS
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include "overlay.h"
#include "resource.h"
#include "sdk/config.h"
#include "sdk/memory.h"

#include "features/aimbot.h"
#include "features/bhop.h"
#include "features/bomb.h"
#include "features/esp.h"
#include "features/nospread.h"
#include "features/radar.h"
#include "features/rcs.h"
#include "features/spectatorlist.h"
#include "features/triggerbot.h"
#include "sdk/fetcher.h"

const std::string CONFIG_VERSION = "v2.8";

void SaveConfig() {
  std::ofstream file("config.cfg");
  if (!file.is_open())
    return;

  file << CONFIG_VERSION << "\n";
  file << Settings::visual_esp << " " << Settings::visual_box << " "
       << Settings::visual_names << " ";
  file << Settings::visual_hp_bar << " " << Settings::visual_tracers << " "
       << Settings::visual_skeleton << " ";
  file << Settings::visual_weapon << " " << Settings::visual_ammo << " "
       << Settings::visual_team_check << " " << Settings::visual_chams << " "
       << Settings::visual_c4 << " " << Settings::visual_offscreen << " "
       << Settings::visual_crosshair << "\n";

  file << Settings::visual_box_color[0] << " " << Settings::visual_box_color[1]
       << " " << Settings::visual_box_color[2] << "\n";
  file << Settings::visual_chams_color[0] << " "
       << Settings::visual_chams_color[1] << " "
       << Settings::visual_chams_color[2] << "\n";
  file << Settings::visual_tracers_color[0] << " "
       << Settings::visual_tracers_color[1] << " "
       << Settings::visual_tracers_color[2] << "\n";
  file << Settings::visual_crosshair_color[0] << " "
       << Settings::visual_crosshair_color[1] << " "
       << Settings::visual_crosshair_color[2] << " "
       << Settings::visual_crosshair_size << "\n";
  file << Settings::ui_theme_color[0] << " " << Settings::ui_theme_color[1]
       << " " << Settings::ui_theme_color[2] << " " << Settings::ui_transparency
       << "\n";

  file << Settings::target_enabled << " " << Settings::target_legit << " "
       << Settings::target_visible_check << " " << Settings::target_fov_range
       << " ";
  file << Settings::target_smooth_factor << " " << Settings::target_key_code
       << " " << Settings::target_bone_idx << " ";
  file << Settings::no_spread_toggle << " " << Settings::trigger_active << " "
       << Settings::trigger_reaction_ms << " " << Settings::aimbot_humanized
       << " " << Settings::aimbot_jitter_scale << " "
       << Settings::draw_fov_circle << " " << Settings::target_smoothing << " "
       << Settings::aim_auto_tab << "\n";

  file << Settings::rcs_enabled << " " << Settings::rcs_auto << " "
       << Settings::rcs_scale_x << " " << Settings::rcs_scale_y << " "
       << Settings::rcs_smooth << "\n";

  for (int i = 0; i < 4; i++) {
    file << Settings::weapon_configs[i].enabled << " "
         << Settings::weapon_configs[i].fov << " "
         << Settings::weapon_configs[i].smooth << " "
         << Settings::weapon_configs[i].bone << "\n";
  }

  file << Settings::auto_hop << " " << Settings::draw_spectators << " "
       << Settings::draw_watermark << " " << Settings::safety_lock << " "
       << Settings::radar_rotating << " " << Settings::sensitivity << " ";
  file << Settings::visual_radar << " " << Settings::radar_pos_x << " "
       << Settings::radar_pos_y << " " << Settings::radar_size << " "
       << Settings::radar_scale << "\n";

  file.close();
}

void LoadConfig() {
  std::ifstream file("config.cfg");
  if (!file.is_open())
    return;

  std::string version;
  file >> version;

  file >> Settings::visual_esp >> Settings::visual_box >>
      Settings::visual_names;
  file >> Settings::visual_hp_bar >> Settings::visual_tracers >>
      Settings::visual_skeleton;
  file >> Settings::visual_weapon >> Settings::visual_ammo >>
      Settings::visual_team_check >> Settings::visual_chams >>
      Settings::visual_c4 >> Settings::visual_offscreen >>
      Settings::visual_crosshair;

  file >> Settings::visual_box_color[0] >> Settings::visual_box_color[1] >>
      Settings::visual_box_color[2];
  file >> Settings::visual_box_color_hidden[0] >>
      Settings::visual_box_color_hidden[1] >>
      Settings::visual_box_color_hidden[2];
  file >> Settings::visual_skeleton_color[0] >>
      Settings::visual_skeleton_color[1] >> Settings::visual_skeleton_color[2];
  file >> Settings::visual_skeleton_color_hidden[0] >>
      Settings::visual_skeleton_color_hidden[1] >>
      Settings::visual_skeleton_color_hidden[2];
  file >> Settings::visual_chams_color[0] >> Settings::visual_chams_color[1] >>
      Settings::visual_chams_color[2];
  file >> Settings::visual_tracers_color[0] >>
      Settings::visual_tracers_color[1] >> Settings::visual_tracers_color[2];
  file >> Settings::visual_crosshair_color[0] >>
      Settings::visual_crosshair_color[1] >>
      Settings::visual_crosshair_color[2] >> Settings::visual_crosshair_size;
  file >> Settings::ui_theme_color[0] >> Settings::ui_theme_color[1] >>
      Settings::ui_theme_color[2] >> Settings::ui_transparency;

  file >> Settings::target_enabled >> Settings::target_legit >>
      Settings::target_visible_check >> Settings::target_fov_range;
  file >> Settings::target_smooth_factor >> Settings::target_key_code >>
      Settings::target_bone_idx;
  file >> Settings::no_spread_toggle >> Settings::trigger_active >>
      Settings::trigger_reaction_ms >> Settings::aimbot_humanized >>
      Settings::aimbot_jitter_scale >> Settings::draw_fov_circle >>
      Settings::target_smoothing >> Settings::aim_auto_tab;

  file >> Settings::rcs_enabled >> Settings::rcs_auto >>
      Settings::rcs_scale_x >> Settings::rcs_scale_y >> Settings::rcs_smooth;

  for (int i = 0; i < 4; i++) {
    file >> Settings::weapon_configs[i].enabled >>
        Settings::weapon_configs[i].fov >> Settings::weapon_configs[i].smooth >>
        Settings::weapon_configs[i].bone;
  }

  file >> Settings::auto_hop >> Settings::draw_spectators >>
      Settings::draw_watermark >> Settings::safety_lock >>
      Settings::radar_rotating >> Settings::sensitivity;
  file >> Settings::visual_radar >> Settings::radar_pos_x >>
      Settings::radar_pos_y >> Settings::radar_size >> Settings::radar_scale;

  file.close();
}

bool CustomButton(const char *label, bool active,
                  const ImVec2 &size_arg = ImVec2(0, 0)) {
  ImGuiWindow *window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext &g = *ImGui::GetCurrentContext();
  const ImGuiStyle &style = g.Style;
  const ImGuiID id = window->GetID(label);
  const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

  ImVec2 pos = window->DC.CursorPos;
  ImVec2 size =
      ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
                          label_size.y + style.FramePadding.y * 2.0f);

  const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
  ImGui::ItemSize(size, style.FramePadding.y);
  if (!ImGui::ItemAdd(bb, id))
    return false;

  bool hovered, held;
  bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

  static std::map<ImGuiID, float> anims;
  float &anim = anims[id];
  float target = active ? 1.0f : (hovered ? 0.7f : 0.0f);
  anim = ImLerp(anim, target, g.IO.DeltaTime * 8.0f);

  if (active) {
    window->DrawList->AddRectFilledMultiColor(
        bb.Min, bb.Max, IM_COL32(56, 26, 117, 255), IM_COL32(76, 36, 157, 255),
        IM_COL32(96, 46, 197, 255), IM_COL32(76, 36, 157, 255));
    window->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(156, 96, 255, 180), 8.0f,
                              0, 2.0f);
  } else {
    ImU32 col1 =
        ImGui::GetColorU32(ImLerp(ImVec4(0.13f, 0.13f, 0.13f, 1.0f),
                                  ImVec4(0.25f, 0.15f, 0.35f, 1.0f), anim));
    ImU32 col2 =
        ImGui::GetColorU32(ImLerp(ImVec4(0.13f, 0.13f, 0.13f, 1.0f),
                                  ImVec4(0.30f, 0.18f, 0.42f, 1.0f), anim));
    window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max, col1, col2, col2,
                                              col1);
    if (anim > 0.1f)
      window->DrawList->AddRect(bb.Min, bb.Max,
                                IM_COL32(100, 60, 140, (int)(anim * 100)), 8.0f,
                                0, 1.0f);
  }

  ImVec4 text_col =
      active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, text_col);
  ImGui::RenderTextClipped(
      ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y),
      ImVec2(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y),
      label, NULL, &label_size, style.ButtonTextAlign, &bb);
  ImGui::PopStyleColor();

  return pressed;
}

bool TabButton(const char *label, bool active) {
  return CustomButton(label, active, ImVec2(120, 40));
}

void UpdateAutoWeaponGroup() {
  if (!Settings::aim_auto_tab)
    return;

  uintptr_t local_pawn = Game::GetLocalPlayerPawn();
  if (!local_pawn)
    return;

  uintptr_t active_weapon = Game::GetActiveWeapon(local_pawn);
  if (!active_weapon)
    return;

  uint16_t item_idx = Game::GetItemDefinitionIndex(active_weapon);
  Settings::current_weapon_type = Game::GetWeaponType(item_idx);
}

void CombatThread() {
  while (true) {
    CombatMod::ApplyAimbot();
    CombatMod::ApplyRCS();
    CombatMod::ApplyTrigger();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void RenderMenu() {
  if (!Settings::menu_visible)
    return;

  ImGui::SetNextWindowSize(ImVec2(680, 500), ImGuiCond_Once);

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.98f));
  ImGui::Begin("Khytt External", &Settings::menu_visible,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

  ImVec2 p = ImGui::GetWindowPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // Header Gradient
  dl->AddRectFilledMultiColor(
      p, ImVec2(p.x + 680, p.y + 60), IM_COL32(35, 15, 65, 255),
      IM_COL32(45, 20, 85, 255), IM_COL32(25, 10, 50, 255),
      IM_COL32(30, 12, 60, 255));
  dl->AddLine(ImVec2(p.x, p.y + 60), ImVec2(p.x + 680, p.y + 60),
              IM_COL32(150, 100, 255, 100));

  ImGui::SetCursorPos(ImVec2(20, 15));
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
  ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "KHYTT EXTERNAL v2.8");
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::SetCursorPosX(620);
  if (ImGui::Button("X", ImVec2(40, 30)))
    Settings::menu_visible = false;

  ImGui::SetCursorPos(ImVec2(0, 60));
  ImGui::BeginChild("Sidebar", ImVec2(160, 440), false,
                    ImGuiWindowFlags_NoScrollbar);
  dl->AddRectFilled(ImVec2(p.x, p.y + 60), ImVec2(p.x + 160, p.y + 500),
                    IM_COL32(15, 15, 20, 255));

  static int current_tab = 0;
  ImGui::SetCursorPos(ImVec2(10, 20));
  if (TabButton("Visuals", current_tab == 0))
    current_tab = 0;
  ImGui::SetCursorPos(ImVec2(10, 70));
  if (TabButton("Combat", current_tab == 1))
    current_tab = 1;
  ImGui::SetCursorPos(ImVec2(10, 120));
  if (TabButton("Misc", current_tab == 2))
    current_tab = 2;

  ImGui::SetCursorPos(ImVec2(10, 340));
  if (CustomButton("Save Config", false, ImVec2(140, 35)))
    SaveConfig();
  ImGui::SetCursorPos(ImVec2(10, 385));
  if (CustomButton("Load Config", false, ImVec2(140, 35)))
    LoadConfig();

  ImGui::EndChild();

  // Content area
  ImGui::SetCursorPos(ImVec2(180, 80));
  ImGui::BeginChild("Content", ImVec2(480, 400), false);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 15));

  if (current_tab == 0) {
    ImGui::TextDisabled("ESP CONFIGURATION");
    ImGui::Separator();
    ImGui::Checkbox("Enable Visuals", &Settings::visual_esp);
    ImGui::Columns(2, NULL, false);
    ImGui::Checkbox("Box ESP", &Settings::visual_box);
    ImGui::Checkbox("Names", &Settings::visual_names);
    ImGui::Checkbox("Health Bar", &Settings::visual_hp_bar);
    ImGui::Checkbox("Skeleton", &Settings::visual_skeleton);
    ImGui::Checkbox("Teammates", &Settings::visual_team_check);
    ImGui::NextColumn();
    ImGui::Checkbox("Weapon Info", &Settings::visual_weapon);
    ImGui::Checkbox("Snaplines", &Settings::visual_tracers);
    ImGui::Checkbox("Crosshair", &Settings::visual_crosshair);
    ImGui::Checkbox("Radar", &Settings::visual_radar);
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::TextDisabled("COLORS & APPEARANCE");

    // Core Visual Colors
    ImGui::Text("Visible Styles:");
    ImGui::ColorEdit3("Box Color", Settings::visual_box_color,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Skeleton Color", Settings::visual_skeleton_color,
                      ImGuiColorEditFlags_NoInputs);

    // Hidden Visual Colors
    ImGui::Text("Occluded Styles:");
    ImGui::ColorEdit3("Hidden Box", Settings::visual_box_color_hidden,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Hidden Skeleton", Settings::visual_skeleton_color_hidden,
                      ImGuiColorEditFlags_NoInputs);

    ImGui::Separator();
    ImGui::ColorEdit3("Tracer Color", Settings::visual_tracers_color,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Crosshair Color", Settings::visual_crosshair_color,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat("Crosshair Size", &Settings::visual_crosshair_size, 1.0f,
                       20.0f);
    ImGui::SliderFloat("Menu Opacity", &Settings::ui_transparency, 0.1f, 1.0f);
  } else if (current_tab == 1) {
    ImGui::TextDisabled("COMBAT CONFIGURATION");
    ImGui::Separator();

    static const char *weapon_groups[] = {"Rifles", "Pistols", "Snipers",
                                          "SMGs"};
    ImGui::Text("Active Group:");
    ImGui::SameLine();
    ImGui::PushItemWidth(150);
    ImGui::Combo("##weapon_group", &Settings::current_weapon_type,
                 weapon_groups, IM_ARRAYSIZE(weapon_groups));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-Switch", &Settings::aim_auto_tab);

    auto &cfg = Settings::weapon_configs[Settings::current_weapon_type];

    ImGui::Checkbox("Master Enable", &Settings::target_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Legit Mode", &Settings::target_legit);
    ImGui::SameLine();

    static const char *keys[] = {"Mouse 4", "Mouse 5", "ALT",
                                 "SHIFT",   "V",       "F"};
    static int key_codes[] = {0x05, 0x06, 0x12, 0x10, 0x56, 0x46};

    ImGui::PushItemWidth(100);
    if (ImGui::Combo("Key", &Settings::target_key_idx, keys,
                     IM_ARRAYSIZE(keys))) {
      Settings::target_key_code = key_codes[Settings::target_key_idx];
    }
    ImGui::PopItemWidth();

    ImGui::Columns(2, NULL, false);
    ImGui::Checkbox("Visible Only", &Settings::target_visible_check);
    ImGui::Checkbox("Draw FOV", &Settings::draw_fov_circle);
    ImGui::Checkbox("Smooth Aim", &Settings::target_smoothing);
    ImGui::Checkbox("Humanized", &Settings::aimbot_humanized);

    ImGui::NextColumn();
    static const char *bones[] = {"Head", "Neck", "Chest", "Stomach"};
    ImGui::Text("Target Bone:");
    ImGui::PushItemWidth(-1);
    ImGui::Combo("##bone", &cfg.bone, bones, IM_ARRAYSIZE(bones));
    ImGui::PopItemWidth();
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::SliderFloat("FOV Range", &cfg.fov, 1.0f, 30.0f, "%.1f");
    ImGui::SliderFloat("Smooth Factor", &cfg.smooth, 1.0f, 20.0f, "%.1f");

    if (Settings::aimbot_humanized) {
      ImGui::SliderFloat("Jitter Scale", &Settings::aimbot_jitter_scale, 0.0f,
                         2.0f);
    }

    ImGui::Separator();
    ImGui::TextDisabled("RECOIL & TRIGGER");
    ImGui::Checkbox("Recoil Control (RCS)", &Settings::rcs_enabled);
    if (Settings::rcs_enabled) {
      ImGui::SliderFloat("Horizontal", &Settings::rcs_scale_x, 0.0f, 2.0f);
      ImGui::SliderFloat("Vertical", &Settings::rcs_scale_y, 0.0f, 2.0f);
    }

    ImGui::Checkbox("Triggerbot", &Settings::trigger_active);
    if (Settings::trigger_active) {
      ImGui::SliderInt("Reaction Delay", &Settings::trigger_reaction_ms, 0, 500,
                       "%d ms");
    }
  } else if (current_tab == 2) {
    ImGui::TextDisabled("EXTRAS & MISC");
    ImGui::Separator();
    ImGui::Checkbox("Automatic Bunnyhop", &Settings::auto_hop);
    ImGui::Checkbox("Spectator List", &Settings::draw_spectators);
    ImGui::Checkbox("Watermark", &Settings::draw_watermark);
    ImGui::Checkbox("Safety Lock", &Settings::safety_lock);

    ImGui::Separator();
    ImGui::TextDisabled("GAME SETTINGS");
    ImGui::SliderFloat("In-game Sensitivity", &Settings::sensitivity, 0.1f,
                       10.0f);
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::End();
  ImGui::PopStyleColor();
}

int main() {
  SetConsoleTitleA("External v2.8");
  std::cout << "[*] Searching for CS2..." << std::endl;

  while (!Memory::Initialize()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // sync offsets
  Fetcher::UpdateOffsets();

  std::cout << "[info] initialization complete." << std::endl;

  if (!Visuals::Initialize()) {
    MessageBoxA(NULL, "Failed to initialize Visuals system", "Error",
                MB_OK | MB_ICONERROR);
    return 1;
  }

  std::thread esp_worker(Visuals::CalculateESP);
  esp_worker.detach();

  std::thread combat_worker(CombatThread);
  combat_worker.detach();

  Overlay::CreateOverlay(L"Khytt CS2 External");
  if (!Overlay::Initialize()) {
    MessageBoxA(NULL, "Failed to initialize D3D11 Overlay", "Error",
                MB_OK | MB_ICONERROR);
    Visuals::Cleanup();
    return 1;
  }

  std::cout << "[info] system running." << std::endl;

  // Auto-focus CS2
  HWND game_hwnd = FindWindowA("SDL_app", "Counter-Strike 2");
  if (game_hwnd) {
    SetForegroundWindow(game_hwnd);
    SetActiveWindow(game_hwnd);
  }

  Visuals::TriggerNotification();
  Overlay::Run([]() {
    UpdateAutoWeaponGroup();
    RenderMenu();
  });

  Visuals::Cleanup();
  Overlay::Cleanup();

  return 0;
}
