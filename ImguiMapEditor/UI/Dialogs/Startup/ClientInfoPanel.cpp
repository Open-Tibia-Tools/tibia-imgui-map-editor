#include "ClientInfoPanel.h"
#include "UI/Core/Theme.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

void ClientInfoPanel::render() {
  // Panel header
  ImGui::TextColored(SC::HEADER_TEXT, "Client information");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (client_info_.version > 0) {
    // Client Name
    ImGui::TextColored(SC::LABEL, ICON_FA_TAG " Client Name");
    if (!client_info_.client_name.empty()) {
      ImGui::TextColored(SC::VALUE, "%s", client_info_.client_name.c_str());
    } else {
      ImGui::TextColored(SC::VALUE, "%s",
                         client_info_.version_string.c_str());
    }
    ImGui::Spacing();

    // Client Version
    ImGui::TextColored(SC::LABEL, ICON_FA_CODE_BRANCH " Client Version");
    ImGui::TextColored(SC::VALUE, "%s", client_info_.version_string.c_str());
    ImGui::Spacing();

    // Data Directory
    ImGui::TextColored(SC::LABEL, ICON_FA_FOLDER " Data Directory");
    if (!client_info_.data_directory.empty()) {
      ImGui::TextColored(SC::VALUE, "%s",
                         client_info_.data_directory.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(Not set)");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // OTBM Version (compare with map)
    bool otbm_match = (client_info_.otbm_version == map_info_.otbm_version);
    ImGui::TextColored(SC::LABEL, ICON_FA_FILE_CODE " OTBM Version");
    ImGui::TextColored(otbm_match ? SC::SAVED : SC::DANGER, "%u",
                       client_info_.otbm_version);
    ImGui::Spacing();

    // Items Major Version
    bool major_match =
        (client_info_.items_major_version == map_info_.items_major_version);
    ImGui::TextColored(SC::LABEL, ICON_FA_CUBES " Items Major Version");
    ImGui::TextColored(major_match ? SC::SAVED : SC::DANGER, "%u",
                       client_info_.items_major_version);
    ImGui::Spacing();

    // Items Minor Version
    bool minor_match =
        (client_info_.items_minor_version == map_info_.items_minor_version);
    ImGui::TextColored(SC::LABEL, ICON_FA_CUBE " Items Minor Version");
    ImGui::TextColored(minor_match ? SC::SAVED : SC::DANGER, "%u",
                       client_info_.items_minor_version);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // DAT Signature
    ImGui::TextColored(SC::LABEL, ICON_FA_FINGERPRINT " DAT Signature");
    if (!client_info_.dat_signature.empty()) {
      ImGui::TextColored(SC::VALUE, "%s", client_info_.dat_signature.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(Unknown)");
    }
    ImGui::Spacing();

    // SPR Signature
    ImGui::TextColored(SC::LABEL, ICON_FA_IMAGE " SPR Signature");
    if (!client_info_.spr_signature.empty()) {
      ImGui::TextColored(SC::VALUE, "%s", client_info_.spr_signature.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(Unknown)");
    }
    ImGui::Spacing();

    // Description
    ImGui::TextColored(SC::LABEL, ICON_FA_FILE_LINES " Description");
    if (!client_info_.description.empty()) {
      ImGui::TextWrapped("%s", client_info_.description.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(No description)");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    ImGui::TextColored(SC::LABEL, ICON_FA_CIRCLE_CHECK " Status");
    if (client_info_.signatures_match) {
      ImGui::TextColored(SC::SAVED, "%s", client_info_.status.c_str());
    } else {
      ImGui::TextColored(SC::WARNING, "%s",
                         client_info_.status.c_str());
    }

    // Signature mismatch warning
    if (signature_mismatch_) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, SC::WARNING);
      ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION " %s",
                         signature_mismatch_message_.c_str());
      ImGui::PopStyleColor();
    }

    // Client not configured warning
    if (client_not_configured_) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, SC::DANGER);
      ImGui::TextWrapped(
          ICON_FA_TRIANGLE_EXCLAMATION
          " Client not configured. Use 'Client Configuration' to add the "
          "client data path.");
      ImGui::PopStyleColor();
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(SC::LABEL,
                       "Select a client to view info");
  }
}

} // namespace UI
} // namespace MapEditor
