// Prevent GLFW from including OpenGL headers (glad provides them)
#define GLFW_INCLUDE_NONE
#include "PreferencesDialog.h"
#include "../Utils/StringCopy.h"
#include "../ext/imhotkey/imHotKey.h"
#include "IO/HotkeyJsonReader.h"
#include "Presentation/NotificationHelper.h"
#include "Services/AppSettings.h"
#include "Services/HotkeyRegistry.h"
#include "Services/SecondaryClientData.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <glad/glad.h>
#include <imgui.h>
#include <nfd.hpp>

namespace MapEditor {
namespace UI {

void PreferencesDialog::show() { should_open_ = true; }

PreferencesDialog::Result PreferencesDialog::render() {
  Result result = Result::None;

  if (should_open_) {
    ImGui::OpenPopup("Preferences###PreferencesDialog");
    should_open_ = false;
    is_open_ = true;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(550, 450), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Preferences###PreferencesDialog", nullptr, 0)) {
    // Tab bar for different categories
    if (ImGui::BeginTabBar("PreferencesTabs")) {
      if (ImGui::BeginTabItem(ICON_FA_GEAR " General")) {
        ImGui::Spacing();
        ImGui::TextDisabled(ICON_FA_HAMMER " Coming Soon");
        ImGui::Spacing();
        ImGui::Text(
            "General preferences will be available in a future update.");
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_PEN " Editor")) {
        renderEditorTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_DISPLAY " Graphics")) {
        ImGui::Spacing();
        ImGui::TextDisabled(ICON_FA_HAMMER " Coming Soon");
        ImGui::Spacing();
        ImGui::Text(
            "Graphics preferences will be available in a future update.");
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_KEYBOARD " Hotkeys")) {
        renderHotkeysTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_CLONE " Secondary Client")) {
        renderSecondaryClientTab();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    // Close button at bottom
    float button_width = 100.0f;
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    ImGui::Separator();
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
    if (ImGui::Button(ICON_FA_CHECK " Close", ImVec2(button_width, 0))) {
      result = Result::Closed;
      ImGui::CloseCurrentPopup();
      is_open_ = false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      result = Result::Closed;
      ImGui::CloseCurrentPopup();
      is_open_ = false;
    }

    ImGui::EndPopup();
  } else if (is_open_) {
    is_open_ = false;
    result = Result::Closed;
  }

  return result;
}

void PreferencesDialog::renderEditorTab() {
  ImGui::Spacing();
  ImGui::TextDisabled("General editor settings will be added here.");
}

void PreferencesDialog::renderSecondaryClientTab() {
  ImGui::Spacing();

  // Description
  ImGui::TextWrapped(
      "Load a secondary client to visualize items missing from your primary "
      "client. "
      "Items from the secondary client will render with a red tint.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Check if secondary client is already loaded
  auto *sec_client = secondary_client_.get();
  bool is_loaded = sec_client && sec_client->isLoaded();

  if (is_loaded) {
    // Show loaded status with green indicator
    bool is_active = sec_client->isActive();

    if (is_active) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                         ICON_FA_CHECK " Secondary Client Active");
    } else {
      ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f),
                         ICON_FA_PAUSE " Secondary Client Loaded (Inactive)");
    }

    ImGui::Spacing();

    // Show folder path
    ImGui::Text("Folder: %s", sec_client->getFolderPath().string().c_str());
    ImGui::Text("Version: %u.%02u", sec_client->getClientVersion() / 100,
                sec_client->getClientVersion() % 100);
    ImGui::Text("Items: %zu", sec_client->getItemCount());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Activate/Deactivate toggle
    bool active = is_active;
    if (ImGui::Checkbox("Active", &active)) {
      if (on_toggle_secondary_) {
        on_toggle_secondary_(active);
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(When inactive, missing items show as placeholders)");

    ImGui::Spacing();

    // Visual settings (only when active)
    if (is_active) {
      ImGui::Text("Visual Settings:");

      // Tint Intensity slider
      float tint = sec_client->getTintIntensity();
      ImGui::SetNextItemWidth(200);
      if (ImGui::SliderFloat("Tint Intensity", &tint, 0.0f, 1.0f, "%.2f")) {
        sec_client->setTintIntensity(tint);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(Red overlay strength)");

      // Alpha/Transparency slider
      float alpha = sec_client->getAlphaMultiplier();
      ImGui::SetNextItemWidth(200);
      if (ImGui::SliderFloat("Opacity", &alpha, 0.3f, 1.0f, "%.2f")) {
        sec_client->setAlphaMultiplier(alpha);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(Item transparency)");

      ImGui::Spacing();
    }

    // Unload button
    if (ImGui::Button(ICON_FA_TRASH " Unload Secondary Client",
                      ImVec2(-1, 30))) {
      if (on_unload_secondary_) {
        on_unload_secondary_();
        secondary_error_.clear();
      }
    }
  } else {
    // Input field for folder
    ImGui::Text("Client Folder (containing Tibia.dat, Tibia.spr, items.otb):");
    ImGui::SetNextItemWidth(-50);
    ImGui::InputText("##FolderPath", secondary_folder_path_,
                     sizeof(secondary_folder_path_));
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##BrowseFolder")) {
      NFD::UniquePath outPath;
      if (NFD::PickFolder(outPath) == NFD_OKAY) {
        ::MapEditor::Utils::copyTruncate(secondary_folder_path_, outPath.get());
      }
    }

    ImGui::TextDisabled(
        "Client version will be auto-detected from DAT/SPR signatures.");

    ImGui::Spacing();

    // Error display
    if (!secondary_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         ICON_FA_TRIANGLE_EXCLAMATION " %s",
                         secondary_error_.c_str());
      ImGui::Spacing();
    }

    // Load button
    bool can_load = secondary_folder_path_[0] != '\0';
    ImGui::BeginDisabled(!can_load);
    if (ImGui::Button(ICON_FA_DOWNLOAD " Load Secondary Client",
                      ImVec2(-1, 30))) {
      if (on_load_secondary_) {
        secondary_error_.clear();
        bool success =
            on_load_secondary_(std::filesystem::path(secondary_folder_path_));

        if (!success) {
          secondary_error_ = "Failed to load. Check folder contains Tibia.dat, "
                             "Tibia.spr, items.otb";
        }
      }
    }
    ImGui::EndDisabled();
  }
}

void PreferencesDialog::renderHotkeysTab() {
  ImGui::Spacing();

  if (!hotkey_registry_) {
    ImGui::TextDisabled("Hotkey registry not available.");
    return;
  }

  ImGui::TextWrapped("Configure keyboard shortcuts. Click an action, then use "
                     "the keyboard editor to assign new keys.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Static storage for editable hotkeys
  static std::vector<ImHotKey::HotKey> hotkeys;
  static std::vector<std::string> names;
  static std::vector<std::string> descriptions;
  static bool initialized = false;

  // Reload from registry when needed
  auto reloadFromRegistry = [&]() {
    hotkeys.clear();
    names.clear();
    descriptions.clear();

    const auto &bindings = hotkey_registry_->getAllBindings();
    // Reserve to avoid reallocations which would invalidate c_str() pointers
    names.reserve(bindings.size());
    descriptions.reserve(bindings.size());
    hotkeys.reserve(bindings.size());

    for (const auto &[action_id, binding] : bindings) {
      names.push_back(action_id);
      descriptions.push_back(binding.category);

      ImHotKey::HotKey hk;
      hk.functionName = names.back().c_str();
      hk.functionLib = descriptions.back().c_str();
      hk.key = binding.key;
      hk.mods = binding.mods;
      hk.isMouse = binding.is_mouse;
      hotkeys.push_back(hk);
    }
    initialized = true;
  };

  if (!initialized || hotkeys.empty()) {
    reloadFromRegistry();
  }

  if (hotkeys.empty()) {
    ImGui::Text("No hotkeys configured.");
    return;
  }

  // Button to open keyboard editor
  if (ImGui::Button(ICON_FA_KEYBOARD " Open Keyboard Editor", ImVec2(-1, 35))) {
    ImGui::OpenPopup("HotKeys Editor");
  }

  // Render the imHotKey editor and handle results
  int appliedIndex = -1;
  ImHotKey::EditResult result = ImHotKey::Edit(hotkeys.data(), hotkeys.size(),
                                               "HotKeys Editor", &appliedIndex);

  // When Apply is clicked, sync immediately
  if (result == ImHotKey::EditResult::Applied && appliedIndex >= 0) {
    // Update registry with the changed binding
    Services::HotkeyBinding binding;
    binding.action_id = hotkeys[appliedIndex].functionName;
    binding.key = hotkeys[appliedIndex].key;
    binding.mods = hotkeys[appliedIndex].mods;
    binding.category = hotkeys[appliedIndex].functionLib;
    binding.is_mouse = hotkeys[appliedIndex].isMouse;
    hotkey_registry_->registerBinding(binding);

    // Save to JSON file
    std::vector<std::filesystem::path> save_paths = {"data/hotkeys.json",
                                                     "../data/hotkeys.json"};
    for (const auto &path : save_paths) {
      if (std::filesystem::exists(path.parent_path()) ||
          path.parent_path().empty()) {
        IO::HotkeyJsonReader::save(path, *hotkey_registry_);
        break;
      }
    }

    // Show toast notification
    char shortcut[64];
    ImHotKey::GetHotKeyLib(hotkeys[appliedIndex], shortcut, sizeof(shortcut));
    char msg[128];
    snprintf(msg, sizeof(msg), "Set %s to %s",
             hotkeys[appliedIndex].functionName, shortcut);
    Presentation::showSuccess(std::string("Set ") +
                                  hotkeys[appliedIndex].functionName + " to " +
                                  shortcut,
                              2000);
  }

  ImGui::Spacing();

  // Show current hotkeys - table fills remaining space (minus button height)
  float availHeight =
      ImGui::GetContentRegionAvail().y - 80.0f; // Leave space for button
  if (availHeight > 80.0f) { // Only show if there's enough space
    if (ImGui::BeginChild("HotkeysTableContainer", ImVec2(0, availHeight),
                          true)) {
      if (ImGui::BeginTable("HotkeysTable", 3,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                150);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                                100);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < hotkeys.size(); i++) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s", hotkeys[i].functionName);

          ImGui::TableNextColumn();
          char shortcut[64];
          ImHotKey::GetHotKeyLib(hotkeys[i], shortcut, sizeof(shortcut));
          ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s",
                             shortcut[0] ? shortcut : "(none)");

          ImGui::TableNextColumn();
          ImGui::TextDisabled("%s", hotkeys[i].functionLib);
        }
        ImGui::EndTable();
      }
    }
    ImGui::EndChild();
  }

  ImGui::Spacing();

  // Reset button
  if (ImGui::Button(ICON_FA_ROTATE_LEFT " Reset All to Defaults",
                    ImVec2(-1, 30))) {
    *hotkey_registry_ = Services::HotkeyRegistry::createDefaults();
    reloadFromRegistry();

    // Save defaults to JSON
    std::vector<std::filesystem::path> save_paths = {"data/hotkeys.json",
                                                     "../data/hotkeys.json"};
    for (const auto &path : save_paths) {
      if (std::filesystem::exists(path.parent_path()) ||
          path.parent_path().empty()) {
        IO::HotkeyJsonReader::save(path, *hotkey_registry_);
        break;
      }
    }

    Presentation::showSuccess("Hotkeys reset to defaults", 2000);
  }
}

} // namespace UI
} // namespace MapEditor
