#include "ClientEditModal.h"
#include "../../../Utils/StringCopy.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <imgui.h>
#include <sstream>

namespace MapEditor {
namespace UI {

void ClientEditModal::openForAdd() {
  clearEditData();
  is_new_client_ = true;
  show_modal_ = true;
}

void ClientEditModal::openForEdit(uint32_t version) {
  fillEditData(version);
  is_new_client_ = false;
  show_modal_ = true;
}

void ClientEditModal::render() {
  if (!show_modal_)
    return;

  ImGui::OpenPopup(is_new_client_ ? "Add Client" : "Edit Client");

  ImVec2 modal_size(500, 450);
  ImGui::SetNextWindowSize(modal_size, ImGuiCond_Always);

  if (ImGui::BeginPopupModal(is_new_client_ ? "Add Client" : "Edit Client",
                             &show_modal_, ImGuiWindowFlags_NoResize)) {

    ImGui::Text("Version Number:");
    ImGui::SameLine(150);
    if (is_new_client_) {
      int ver = static_cast<int>(edit_data_.version);
      if (ImGui::InputInt("##version", &ver)) {
        edit_data_.version = static_cast<uint32_t>(std::max(0, ver));
      }
    } else {
      ImGui::Text("%u (read-only)", edit_data_.version);
    }

    ImGui::Text("Name:");
    ImGui::SameLine(150);
    ImGui::InputText("##name", edit_data_.name, sizeof(edit_data_.name));

    ImGui::Text("Description:");
    ImGui::SameLine(150);
    ImGui::InputText("##desc", edit_data_.description,
                     sizeof(edit_data_.description));

    ImGui::Text("Data Directory:");
    ImGui::SameLine(150);
    ImGui::InputText("##datadir", edit_data_.data_directory,
                     sizeof(edit_data_.data_directory));

    ImGui::Separator();
    ImGui::Text("Version Identifiers");

    ImGui::Text("OTB ID:");
    ImGui::SameLine(150);
    int otb_id = static_cast<int>(edit_data_.otb_id);
    if (ImGui::InputInt("##otbid", &otb_id)) {
      edit_data_.otb_id = static_cast<uint32_t>(std::max(0, otb_id));
    }

    ImGui::Text("OTB Major:");
    ImGui::SameLine(150);
    int otb_major = static_cast<int>(edit_data_.otb_major);
    if (ImGui::InputInt("##otbmajor", &otb_major)) {
      edit_data_.otb_major = static_cast<uint32_t>(std::max(0, otb_major));
    }

    ImGui::Text("OTBM Version:");
    ImGui::SameLine(150);
    int otbm_ver = static_cast<int>(edit_data_.otbm_version);
    if (ImGui::InputInt("##otbmver", &otbm_ver)) {
      edit_data_.otbm_version = static_cast<uint32_t>(std::max(0, otbm_ver));
    }

    ImGui::Separator();
    ImGui::Text("Signatures (hex)");

    ImGui::Text("DAT Signature:");
    ImGui::SameLine(150);
    ImGui::InputText("##datsig", edit_data_.dat_signature,
                     sizeof(edit_data_.dat_signature));

    ImGui::Text("SPR Signature:");
    ImGui::SameLine(150);
    ImGui::InputText("##sprsig", edit_data_.spr_signature,
                     sizeof(edit_data_.spr_signature));

    ImGui::Separator();
    ImGui::Text("Client Path:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-80);
    ImGui::InputText("##path", edit_data_.client_path,
                     sizeof(edit_data_.client_path));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("...##browse")) {
      if (on_browse_) {
        on_browse_(edit_data_);
      }
    }

    ImGui::Checkbox("Set as Default", &edit_data_.is_default);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      show_modal_ = false;
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 110);
    if (ImGui::Button(is_new_client_ ? "Add" : "Save", ImVec2(100, 0))) {
      if (saveClient()) {
        show_modal_ = false;
        if (on_save_) {
          on_save_();
        }
      }
    }

    ImGui::EndPopup();
  }
}

void ClientEditModal::fillEditData(uint32_t version) {
  clearEditData();
  if (!registry_)
    return;

  auto *cv = registry_->getVersion(version);
  if (!cv)
    return;

  edit_data_.version = cv->getVersion();
  ::MapEditor::Utils::copyTruncate(edit_data_.name, cv->getName());
  ::MapEditor::Utils::copyTruncate(edit_data_.description, cv->getDescription());
  ::MapEditor::Utils::copyTruncate(edit_data_.data_directory, cv->getDataDirectory());
  ::MapEditor::Utils::copyTruncate(edit_data_.client_path, cv->getClientPath().string());

  edit_data_.otb_id = cv->getOtbVersion();
  edit_data_.otb_major = cv->getOtbMajor();
  edit_data_.otbm_version = cv->getOtbmVersion();

  std::snprintf(edit_data_.dat_signature, sizeof(edit_data_.dat_signature),
                "%08X", cv->getDatSignature());
  std::snprintf(edit_data_.spr_signature, sizeof(edit_data_.spr_signature),
                "%08X", cv->getSprSignature());

  edit_data_.is_default = cv->isDefault();
}

void ClientEditModal::clearEditData() { edit_data_ = ClientEditData{}; }

bool ClientEditModal::saveClient() {
  if (!registry_ || edit_data_.version == 0) {
    return false;
  }

  Domain::ClientVersion cv(edit_data_.version, edit_data_.name,
                           edit_data_.otb_id);
  cv.setDescription(edit_data_.description);
  cv.setDataDirectory(edit_data_.data_directory);
  cv.setOtbMajor(edit_data_.otb_major);
  cv.setOtbmVersion(edit_data_.otbm_version);
  cv.setClientPath(edit_data_.client_path);
  cv.setDefault(edit_data_.is_default);

  // Parse signatures from hex
  uint32_t dat_sig = 0, spr_sig = 0;
  std::stringstream ss_dat, ss_spr;
  ss_dat << std::hex << edit_data_.dat_signature;
  ss_dat >> dat_sig;
  ss_spr << std::hex << edit_data_.spr_signature;
  ss_spr >> spr_sig;
  cv.setDatSignature(dat_sig);
  cv.setSprSignature(spr_sig);

  if (is_new_client_) {
    registry_->addClient(cv);
  } else {
    registry_->updateClient(edit_data_.version, cv);
  }

  if (edit_data_.is_default) {
    registry_->setDefaultVersion(edit_data_.version);
  }

  return true;
}

} // namespace UI
} // namespace MapEditor
