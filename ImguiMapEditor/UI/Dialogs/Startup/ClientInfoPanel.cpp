#include "ClientInfoPanel.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

void ClientInfoPanel::render() {
  // Panel header
  ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "Client information");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (client_info_.version > 0) {
    const ImVec4 match_color(0.3f, 0.85f, 0.5f, 1.0f);
    const ImVec4 mismatch_color(0.9f, 0.4f, 0.4f, 1.0f);
    const ImVec4 label_color(0.55f, 0.58f, 0.62f, 1.0f);
    const ImVec4 value_color(0.95f, 0.95f, 0.95f, 1.0f);
    const ImVec4 empty_color(0.4f, 0.42f, 0.45f, 1.0f);

    // Client Name
    ImGui::TextColored(label_color, ICON_FA_TAG " Client Name");
    if (!client_info_.client_name.empty()) {
      ImGui::TextColored(value_color, "%s", client_info_.client_name.c_str());
    } else {
      ImGui::TextColored(value_color, "%s",
                         client_info_.version_string.c_str());
    }
    ImGui::Spacing();

    // Client Version
    ImGui::TextColored(label_color, ICON_FA_CODE_BRANCH " Client Version");
    ImGui::TextColored(value_color, "%s", client_info_.version_string.c_str());
    ImGui::Spacing();

    // Data Directory
    ImGui::TextColored(label_color, ICON_FA_FOLDER " Data Directory");
    if (!client_info_.data_directory.empty()) {
      ImGui::TextColored(value_color, "%s",
                         client_info_.data_directory.c_str());
    } else {
      ImGui::TextColored(empty_color, "(Not set)");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // OTBM Version (compare with map)
    bool const otbm_match = (client_info_.otbm_version == map_info_.otbm_version);
    ImGui::TextColored(label_color, ICON_FA_FILE_CODE " OTBM Version");
    ImGui::TextColored(otbm_match ? match_color : mismatch_color, "%u",
                       client_info_.otbm_version);
    ImGui::Spacing();

    // Items Major Version
    bool const major_match =
        (client_info_.items_major_version == map_info_.items_major_version);
    ImGui::TextColored(label_color, ICON_FA_CUBES " Items Major Version");
    ImGui::TextColored(major_match ? match_color : mismatch_color, "%u",
                       client_info_.items_major_version);
    ImGui::Spacing();

    // Items Minor Version
    bool const minor_match =
        (client_info_.items_minor_version == map_info_.items_minor_version);
    ImGui::TextColored(label_color, ICON_FA_CUBE " Items Minor Version");
    ImGui::TextColored(minor_match ? match_color : mismatch_color, "%u",
                       client_info_.items_minor_version);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // DAT Signature
    ImGui::TextColored(label_color, ICON_FA_FINGERPRINT " DAT Signature");
    if (!client_info_.dat_signature.empty()) {
      ImGui::TextColored(value_color, "%s", client_info_.dat_signature.c_str());
    } else {
      ImGui::TextColored(empty_color, "(Unknown)");
    }
    ImGui::Spacing();

    // SPR Signature
    ImGui::TextColored(label_color, ICON_FA_IMAGE " SPR Signature");
    if (!client_info_.spr_signature.empty()) {
      ImGui::TextColored(value_color, "%s", client_info_.spr_signature.c_str());
    } else {
      ImGui::TextColored(empty_color, "(Unknown)");
    }
    ImGui::Spacing();

    // Description
    ImGui::TextColored(label_color, ICON_FA_FILE_LINES " Description");
    if (!client_info_.description.empty()) {
      ImGui::TextWrapped("%s", client_info_.description.c_str());
    } else {
      ImGui::TextColored(empty_color, "(No description)");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    ImGui::TextColored(label_color, ICON_FA_CIRCLE_CHECK " Status");
    if (client_info_.signatures_match) {
      ImGui::TextColored(match_color, "%s", client_info_.status.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.9f, 0.65f, 0.3f, 1.0f), "%s",
                         client_info_.status.c_str());
    }

    // Signature mismatch warning
    if (signature_mismatch_) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.3f, 1.0f));
      ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION " %s",
                         signature_mismatch_message_.c_str());
      ImGui::PopStyleColor();
    }

    // Client not configured warning
    if (client_not_configured_) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
      ImGui::TextWrapped(
          ICON_FA_TRIANGLE_EXCLAMATION
          " Client not configured. Use 'Client Configuration' to add the "
          "client data path.");
      ImGui::PopStyleColor();
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.52f, 0.55f, 1.0f),
                       "Select a client to view info");
  }
}

} // namespace UI
} // namespace MapEditor
