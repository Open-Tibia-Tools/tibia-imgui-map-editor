#include "UI/Panels/OpenSecPanel.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.hpp>


namespace MapEditor {
namespace UI {

OpenSecPanel::OpenSecPanel() = default;

void OpenSecPanel::initialize(Services::ClientVersionRegistry *registry,
                              Services::ClientVersionValidator *validator) {
  registry_ = registry;
  validator_ = validator;
}

OpenSecPanel::RenderResult OpenSecPanel::render(State &state) {
  RenderResult result;

  renderSecFolderSelector(state);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // 2-column layout
  float col_width = ImGui::GetContentRegionAvail().x / 2.0f - 6.0f;
  float content_height = 300.0f;

  // Column 1: Map Info
  ImGui::BeginChild("SecMapInfo", ImVec2(col_width, content_height),
                    ImGuiChildFlags_Borders);
  renderMapInfo(state);
  ImGui::EndChild();

  ImGui::SameLine();

  // Column 2: Client Selection
  ImGui::BeginChild("SecClientInfo", ImVec2(0, content_height),
                    ImGuiChildFlags_Borders);
  renderClientSelector(state);
  ImGui::EndChild();

  return result;
}

void OpenSecPanel::renderSecFolderSelector(State &state) {
  ImGui::Text("SEC Map Folder");
  ImGui::PushItemWidth(-80);

  sec_path_buffer_ = state.sec_folder.string();
  if (ImGui::InputText("##secpath", &sec_path_buffer_)) {
    state.sec_folder = sec_path_buffer_;
    scanSecFolder(state);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Folder containing *.sec sector files");
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse...##sec")) {
    browseForSecFolder(state);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Select folder with .sec files");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_PASTE "##sec")) {
    if (const char *clipboard = ImGui::GetClipboardText()) {
      sec_path_buffer_ = clipboard; // std::string assignment
      state.sec_folder = sec_path_buffer_;
      scanSecFolder(state);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Paste path from clipboard");
  }
}

void OpenSecPanel::browseForSecFolder(State &state) {
  NFD::UniquePath outPath;
  nfdresult_t result = NFD::PickFolder(outPath);
  if (result == NFD_OKAY) {
    state.sec_folder = outPath.get();
    sec_path_buffer_ = state.sec_folder.string();
    scanSecFolder(state);
  }
}

void OpenSecPanel::browseForClientFolder(State &state) {
  NFD::UniquePath outPath;
  nfdresult_t result = NFD::PickFolder(outPath);
  if (result == NFD_OKAY) {
    state.client_path = outPath.get();
    client_path_buffer_ = state.client_path.string();

    if (validator_) {
      uint32_t version = validator_->detectVersion(state.client_path);
      if (registry_ && version > 0) {
        if (auto* cv = registry_->findBestByVersion(version)) {
          state.selected_client_index = cv->getIndex();
        } else {
          state.selected_client_index = 0;
        }
      } else if (version == 0) {
        state.selected_client_index = 0;
      }
    }
    validateClientForSec(state);
  }
}

void OpenSecPanel::scanSecFolder(State &state) {
  state.scan_valid = false;
  state.sector_count = 0;

  if (state.sec_folder.empty() || !std::filesystem::exists(state.sec_folder)) {
    return;
  }

  auto scan_result = IO::SecReader::scanBounds(state.sec_folder);
  if (scan_result.success) {
    state.sector_count = scan_result.sector_count;
    state.sector_x_min = scan_result.sector_x_min;
    state.sector_x_max = scan_result.sector_x_max;
    state.sector_y_min = scan_result.sector_y_min;
    state.sector_y_max = scan_result.sector_y_max;
    state.sector_z_min = scan_result.sector_z_min;
    state.sector_z_max = scan_result.sector_z_max;
    state.scan_valid = true;
  }

  validateClientForSec(state);
}

void OpenSecPanel::validateClientForSec(State &state) {
  state.has_items_srv = false;
  state.paths_valid = false;
  state.validation_error.clear();

  // Check SEC folder first
  if (!state.scan_valid) {
    state.validation_error = "Select a folder with .sec files";
    return;
  }

  if (state.client_path.empty()) {
    state.validation_error = "Select a client folder";
    return;
  }

  // Check for items.srv (required for SEC)
  auto srv_path = state.client_path / "items.srv";
  if (!std::filesystem::exists(srv_path)) {
    state.validation_error = "Missing: items.srv (required for SEC maps)";
    return;
  }

  // Check for DAT/SPR
  auto dat_path = state.client_path / "Tibia.dat";
  auto spr_path = state.client_path / "Tibia.spr";
  if (!std::filesystem::exists(dat_path)) {
    state.validation_error = "Missing: Tibia.dat";
    return;
  }
  if (!std::filesystem::exists(spr_path)) {
    state.validation_error = "Missing: Tibia.spr";
    return;
  }

  state.has_items_srv = true;
  state.paths_valid = true;
}

void OpenSecPanel::renderMapInfo(const State &state) {
  ImGui::Text("SEC MAP INFO");
  ImGui::Spacing();

  if (!ImGui::BeginTable("SecMapProps", 2, ImGuiTableFlags_None))
    return;

  ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

  // Folder Name
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Folder:");
  ImGui::TableNextColumn();
  if (!state.sec_folder.empty()) {
    ImGui::Text("%s", state.sec_folder.filename().string().c_str());
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a folder...");
  }

  // Sector Count
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Sectors:");
  ImGui::TableNextColumn();
  if (state.scan_valid) {
    ImGui::Text("%zu files", state.sector_count);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  // Bounds X
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("X Range:");
  ImGui::TableNextColumn();
  if (state.scan_valid) {
    ImGui::Text("%d - %d (%d sectors)", state.sector_x_min, state.sector_x_max,
                state.sector_x_max - state.sector_x_min + 1);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  // Bounds Y
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Y Range:");
  ImGui::TableNextColumn();
  if (state.scan_valid) {
    ImGui::Text("%d - %d (%d sectors)", state.sector_y_min, state.sector_y_max,
                state.sector_y_max - state.sector_y_min + 1);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  // Floors
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Floors:");
  ImGui::TableNextColumn();
  if (state.scan_valid) {
    ImGui::Text("%d - %d", state.sector_z_min, state.sector_z_max);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  // Estimated Size (tiles)
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Est. Size:");
  ImGui::TableNextColumn();
  if (state.scan_valid) {
    int width = (state.sector_x_max - state.sector_x_min + 1) * 32;
    int height = (state.sector_y_max - state.sector_y_min + 1) * 32;
    ImGui::Text("%d x %d tiles", width, height);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  ImGui::EndTable();
}

void OpenSecPanel::renderClientSelector(State &state) {
  ImGui::Text("CLIENT DATA");
  ImGui::Spacing();

  // Client folder selector
  client_path_buffer_ = state.client_path.string();
  ImGui::PushItemWidth(-80);
  if (ImGui::InputText("##secclientpath", &client_path_buffer_)) {
      state.client_path = client_path_buffer_;
      if (validator_) {
        uint32_t version = validator_->detectVersion(state.client_path);
        if (registry_ && version > 0) {
          if (auto* cv = registry_->findBestByVersion(version)) {
            state.selected_client_index = cv->getIndex();
          } else {
            state.selected_client_index = 0;
          }
        } else if (version == 0) {
          state.selected_client_index = 0;
        }
      }
    validateClientForSec(state);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Path to 7.x client with Tibia.dat, Tibia.spr, items.srv");
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse...##secclient")) {
    browseForClientFolder(state);
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_PASTE "##secclient")) {
    if (const char *clipboard = ImGui::GetClipboardText()) {
      client_path_buffer_ = clipboard; // std::string assignment
      state.client_path = client_path_buffer_;
      if (validator_) {
        uint32_t version = validator_->detectVersion(state.client_path);
        if (registry_ && version > 0) {
          if (auto* cv = registry_->findBestByVersion(version)) {
            state.selected_client_index = cv->getIndex();
          } else {
            state.selected_client_index = 0;
          }
        } else if (version == 0) {
          state.selected_client_index = 0;
        }
      }
      validateClientForSec(state);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Paste path from clipboard");
  }

  ImGui::Spacing();

  if (!ImGui::BeginTable("SecClientData", 2, ImGuiTableFlags_None))
    return;

  ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

  // Detected Version
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("Version:");
  ImGui::TableNextColumn();
  if (state.selected_client_index > 0) {
    uint32_t ver = 0;
    if (registry_) {
      for (const auto* cv : registry_->getAllVersions()) {
        if (cv && cv->getIndex() == state.selected_client_index) {
          ver = cv->getVersion();
          break;
        }
      }
    }
    if (ver > 0)
      ImGui::Text("%u.%02u", ver / 100, ver % 100);
    else
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Auto-detect");
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Auto-detect");
  }

  // items.srv status
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("items.srv:");
  ImGui::TableNextColumn();
  if (state.has_items_srv) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                       ICON_FA_CIRCLE_CHECK " Found");
  } else if (!state.client_path.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       ICON_FA_CIRCLE_XMARK " Missing");
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
  }

  ImGui::EndTable();

  ImGui::Spacing();

  // Status message
  if (state.paths_valid) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                       ICON_FA_CIRCLE_CHECK " Ready to load SEC map");
  } else if (!state.validation_error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s",
                       state.validation_error.c_str());
  }

  // Info box about SEC format
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
                     ICON_FA_CIRCLE_INFO " SEC maps use server IDs");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                     "(Requires items.srv, not items.otb)");
}

} // namespace UI
} // namespace MapEditor
