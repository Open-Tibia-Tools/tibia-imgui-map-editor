#include "UI/Panels/NewMapPanel.h"
#include "Core/Config.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <format>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.hpp>


namespace MapEditor {
namespace UI {

NewMapPanel::NewMapPanel() { name_buffer_ = "Untitled"; }

void NewMapPanel::initialize(Services::ClientVersionRegistry *registry,
                             Services::RecentLocationsService *recent) {
  registry_ = registry;
  recent_ = recent;
}

NewMapPanel::RenderResult NewMapPanel::render(State &state) {
  RenderResult result;

  // Recent clients panel
  if (recent_ && !recent_->getRecentClients().empty()) {
    renderRecentClients(state);
    ImGui::Spacing();
  }

  ImGui::Separator();
  renderClientPathSelector(state);
  ImGui::Spacing();

  renderMapSettings(state);

  return result;
}

void NewMapPanel::renderRecentClients(State &state) {
  ImGui::BeginChild("RecentClientsCard", ImVec2(0, 120),
                    ImGuiChildFlags_Borders);

  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                     ICON_FA_FOLDER_OPEN " RECENT CLIENTS");
  ImGui::Separator();

  for (const auto &entry : recent_->getRecentClients()) {
    bool is_default = (entry.client_index == registry_->getDefaultVersion());

    // Get client name from registry
    std::string version_str;
    if (auto *client = registry_->getVersion(entry.client_index)) {
      version_str = client->getName();
    } else {
      version_str = std::format("{}.{:02}", entry.client_index / 100,
                                entry.client_index % 100);
    }

    ImGui::PushID(&entry);
    if (ImGui::Selectable("", false, 0, ImVec2(0, 24))) {
      state.client_path = entry.path;
      state.selected_client_index = entry.client_index;
      path_buffer_ = entry.path.string();
    }
    // Tooltip for the whole row
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", entry.path.string().c_str());
    }

    ImGui::SameLine(5);
    if (is_default) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FA_STAR);
    } else {
      ImGui::TextDisabled(ICON_FA_FOLDER);
    }
    ImGui::SameLine();
    ImGui::Text("%s", version_str.c_str());
    ImGui::SameLine(140);
    ImGui::TextDisabled("%s", entry.path.string().c_str());

    ImGui::PopID();
  }

  ImGui::EndChild();
}

void NewMapPanel::renderClientPathSelector(State &state) {
  ImGui::Text("Client Data Folder");
  ImGui::TextDisabled("Folder containing Tibia.dat, Tibia.spr, and items.otb");

  path_buffer_ = state.client_path.string();

  ImGui::PushItemWidth(-180);
  if (ImGui::InputText("##path", &path_buffer_)) {
    state.client_path = path_buffer_;
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse...")) {
    NFD::UniquePath outPath;
    nfdresult_t result = NFD::PickFolder(outPath);
    if (result == NFD_OKAY) {
      path_buffer_ = outPath.get();
      state.client_path = path_buffer_;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Select Tibia client directory (containing Tibia.dat/spr)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_PASTE)) {
    if (const char *clipboard = ImGui::GetClipboardText()) {
      path_buffer_ = clipboard;
      state.client_path = path_buffer_;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Paste path from clipboard");
  }
}

void NewMapPanel::renderMapSettings(State &state) {
  ImGui::Separator();
  ImGui::Text("Map Settings");

  ImGui::Text("Map Name");
  name_buffer_ = state.map_name;
  if (ImGui::InputText("##mapname", &name_buffer_)) {
    state.map_name = name_buffer_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Name of the new map (e.g., 'Thais')");
  }

  ImGui::Text("Map Size");
  int width = static_cast<int>(state.map_width);
  int height = static_cast<int>(state.map_height);

  ImGui::PushItemWidth(100);
  if (ImGui::InputInt("Width", &width)) {
    state.map_width = static_cast<uint16_t>(
        std::clamp(width, Config::Map::MIN_SIZE, Config::Map::MAX_SIZE));
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Map width (256-65535)");
  }
  ImGui::SameLine();
  if (ImGui::InputInt("Height", &height)) {
    state.map_height = static_cast<uint16_t>(
        std::clamp(height, Config::Map::MIN_SIZE, Config::Map::MAX_SIZE));
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Map height (256-65535)");
  }
  ImGui::PopItemWidth();
}

} // namespace UI
} // namespace MapEditor
