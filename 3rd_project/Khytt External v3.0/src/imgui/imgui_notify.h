#pragma once
#include "imgui-master/imgui.h"
#include "imgui-master/imgui_internal.h"
#include <chrono>
#include <string>
#include <vector>


enum ImGuiToastType {
  ImGuiToastType_None,
  ImGuiToastType_Success,
  ImGuiToastType_Warning,
  ImGuiToastType_Error,
  ImGuiToastType_Info,
};

struct ImGuiToast {
  ImGuiToastType type = ImGuiToastType_None;
  std::string title;
  std::string content;
  int dismiss_time = 3000;
  std::chrono::system_clock::time_point creation_time =
      std::chrono::system_clock::now();
  float alpha = 0.0f; // Animation state

  ImGuiToast(ImGuiToastType type, int dismiss_time = 3000)
      : type(type), dismiss_time(dismiss_time) {}
  ImGuiToast(ImGuiToastType type, const char *format, ...) : type(type) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    this->content = buffer;
    this->dismiss_time = 3000;
  }
};

namespace ImGui {
inline std::vector<ImGuiToast> notifications;

inline void InsertNotification(const ImGuiToast &toast) {
  notifications.push_back(toast);
}

inline void RenderNotifications() {
  const float padding = 20.0f;
  const float window_width = ImGui::GetIO().DisplaySize.x;
  const float window_height = ImGui::GetIO().DisplaySize.y;

  float current_y = window_height - padding;

  for (int i = 0; i < notifications.size(); i++) {
    auto &toast = notifications[i];

    // Check expiry
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - toast.creation_time)
                        .count();
    if (duration > toast.dismiss_time) {
      notifications.erase(notifications.begin() + i);
      i--;
      continue;
    }

    // Animation
    if (duration < 300) {
      toast.alpha = (float)duration / 300.0f;
    } else if (duration > toast.dismiss_time - 300) {
      toast.alpha = (float)(toast.dismiss_time - duration) / 300.0f;
    } else {
      toast.alpha = 1.0f;
    }

    ImGui::SetNextWindowBgAlpha(0.9f * toast.alpha);
    ImGui::SetNextWindowPos(
        ImVec2(window_width - padding - 300, current_y - 70));
    ImGui::SetNextWindowSize(ImVec2(300, 60));

    char window_name[32];
    sprintf(window_name, "##Notify%d", i);

    ImGui::Begin(window_name, nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);

    ImVec4 text_color = ImVec4(1, 1, 1, toast.alpha);
    ImVec4 title_color;
    const char *title = "Info";

    switch (toast.type) {
    case ImGuiToastType_Success:
      title = "Success";
      title_color = ImVec4(0.2f, 0.8f, 0.2f, toast.alpha);
      break;
    case ImGuiToastType_Warning:
      title = "Warning";
      title_color = ImVec4(0.8f, 0.8f, 0.2f, toast.alpha);
      break;
    case ImGuiToastType_Error:
      title = "Error";
      title_color = ImVec4(0.8f, 0.2f, 0.2f, toast.alpha);
      break;
    case ImGuiToastType_Info:
      title = "Info";
      title_color = ImVec4(0.2f, 0.6f, 1.0f, toast.alpha);
      break;
    default:
      title = "Info";
      title_color = ImVec4(1, 1, 1, toast.alpha);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, title_color);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    ImGui::TextWrapped("%s", toast.content.c_str());
    ImGui::PopStyleColor();

    ImGui::End();

    current_y -= 75;
  }
}
} // namespace ImGui
