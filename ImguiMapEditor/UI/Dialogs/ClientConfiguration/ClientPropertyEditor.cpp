#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <sstream>
#include <vector>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <nfd.hpp>

namespace MapEditor {
namespace UI {

namespace {

constexpr const char *kAutoProps[] = {
    "metadataFile", "spritesFile", "datSignature", "sprSignature",
    "transparency", "extended",  "frameDurations", "frameGroups",
};
constexpr size_t kAutoCount = sizeof(kAutoProps) / sizeof(kAutoProps[0]);

constexpr uint32_t kOtbmVersionMin = 1;
constexpr uint32_t kOtbmVersionMax = 4;

bool isAutoProp(const char *name) {
  for (size_t i = 0; i < kAutoCount; ++i)
    if (std::strcmp(kAutoProps[i], name) == 0)
      return true;
  return false;
}

ImVec4 blend(const ImVec4 &a, const ImVec4 &b, float t) {
  return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t);
}

constexpr ImVec4 kRed = ImVec4(0.6f, 0.15f, 0.15f, 1.0f);
constexpr ImVec4 kYellow = ImVec4(0.6f, 0.5f, 0.0f, 1.0f);
constexpr ImVec4 kGreen = ImVec4(0.15f, 0.55f, 0.15f, 1.0f);
constexpr ImVec4 kTextMuted = ImVec4(0.67f, 0.70f, 0.75f, 1.0f);
constexpr ImVec4 kGreenStatus = ImVec4(0.43f, 0.82f, 0.43f, 1.0f);
constexpr ImVec4 kBlueAccent = ImVec4(0.19f, 0.44f, 0.84f, 1.0f);
constexpr ImVec4 kBlueHover = ImVec4(0.25f, 0.50f, 0.92f, 1.0f);

class ScopedPropertyColor {
public:
  ScopedPropertyColor(const char *property_name,
                      const std::unordered_map<std::string, Domain::PropertyVisualState> *states)
      : states_(states), name_(property_name) {
    if (states_) {
      auto it = states_->find(name_);
      if (it != states_->end()) {
        switch (it->second) {
        case Domain::PropertyVisualState::Saved:
          ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
          pushed_ = true;
          break;
        case Domain::PropertyVisualState::Pending:
          ImGui::PushStyleColor(ImGuiCol_Text, kYellow);
          pushed_ = true;
          break;
        case Domain::PropertyVisualState::Undetected:
          ImGui::PushStyleColor(ImGuiCol_Text, kRed);
          pushed_ = true;
          break;
        default:
          break;
        }
      }
    }
  }
  ~ScopedPropertyColor() {
    if (pushed_)
      ImGui::PopStyleColor();
  }

private:
  const std::unordered_map<std::string, Domain::PropertyVisualState> *states_;
  std::string name_;
  bool pushed_ = false;
};

float labelColumn() { return ImGui::GetContentRegionAvail().x * 0.35f; }

} // namespace

void ClientPropertyEditor::setPropertyState(uint32_t version, const char *name,
                                            Domain::PropertyVisualState state) {
  property_states_[version][name] = state;
}

Domain::PropertyVisualState
ClientPropertyEditor::getPropertyState(uint32_t version, const char *name) const {
  auto vit = property_states_.find(version);
  if (vit != property_states_.end()) {
    auto pit = vit->second.find(name);
    if (pit != vit->second.end())
      return pit->second;
  }
  return Domain::PropertyVisualState::Default;
}

void ClientPropertyEditor::clearPropertyStates(uint32_t version) {
  property_states_.erase(version);
}

void ClientPropertyEditor::markAllPendingAsSaved(uint32_t version) {
  auto vit = property_states_.find(version);
  if (vit != property_states_.end()) {
    for (auto &pair : vit->second) {
      if (pair.second == Domain::PropertyVisualState::Pending)
        pair.second = Domain::PropertyVisualState::Saved;
    }
  }
}

void ClientPropertyEditor::syncFromClient(const Domain::ClientVersion &cv) {
  std::strncpy(name_buf_, cv.getName().c_str(), sizeof(name_buf_) - 1);
  std::strncpy(desc_buf_, cv.getDescription().c_str(), sizeof(desc_buf_) - 1);
  std::strncpy(data_dir_buf_, cv.getDataDirectory().c_str(),
               sizeof(data_dir_buf_) - 1);
  std::strncpy(client_path_buf_, cv.getClientPath().string().c_str(),
               sizeof(client_path_buf_) - 1);
  std::strncpy(metadata_buf_, cv.getMetadataFile().c_str(),
               sizeof(metadata_buf_) - 1);
  std::strncpy(sprites_buf_, cv.getSpritesFile().c_str(),
               sizeof(sprites_buf_) - 1);

  std::stringstream ss;
  ss << std::uppercase << std::hex << cv.getDatSignature();
  std::strncpy(dat_sig_buf_, ss.str().c_str(), sizeof(dat_sig_buf_) - 1);

  ss.str("");
  ss.clear();
  ss << std::uppercase << std::hex << cv.getSprSignature();
  std::strncpy(spr_sig_buf_, ss.str().c_str(), sizeof(spr_sig_buf_) - 1);

  version_int_ = static_cast<int>(cv.getVersion());
  otb_id_int_ = static_cast<int>(cv.getOtbVersion());
  otb_major_int_ = static_cast<int>(cv.getOtbMajor());

  data_source_idx_ = static_cast<int>(cv.getDataSource());

  std::string otbm_str;
  const auto &otbm = cv.getMapVersionsSupported();
  for (size_t i = 0; i < otbm.size(); ++i) {
    otbm_str += std::to_string(otbm[i]);
    if (i < otbm.size() - 1)
      otbm_str += ",";
  }
  std::strncpy(otbm_versions_buf_, otbm_str.c_str(),
               sizeof(otbm_versions_buf_) - 1);

  transparent_bool_ = cv.isTransparent();
  extended_bool_ = cv.isExtended();
  frame_durations_bool_ = cv.hasFrameDurations();
  frame_groups_bool_ = cv.hasFrameGroups();
  is_default_bool_ = cv.isDefault();
}

void ClientPropertyEditor::syncToClient(Domain::ClientVersion &cv) {
  cv.setName(name_buf_);
  cv.setDescription(desc_buf_);
  cv.setDataDirectory(data_dir_buf_);
  cv.setClientPath(client_path_buf_);
  cv.setMetadataFile(metadata_buf_);
  cv.setSpritesFile(sprites_buf_);

  uint32_t dat_sig = 0, spr_sig = 0;
  std::stringstream ss;
  ss << std::hex << dat_sig_buf_;
  ss >> dat_sig;
  cv.setDatSignature(dat_sig);

  ss.str("");
  ss.clear();
  ss << std::hex << spr_sig_buf_;
  ss >> spr_sig;
  cv.setSprSignature(spr_sig);

  cv.setVersion(static_cast<uint32_t>(version_int_));
  cv.setOtbVersion(static_cast<uint32_t>(otb_id_int_));
  cv.setOtbMajor(static_cast<uint32_t>(otb_major_int_));

  cv.setDataSource(static_cast<::MapEditor::Domain::ItemDataSource>(data_source_idx_));

  std::vector<uint32_t> otbm;
  std::istringstream iss(otbm_versions_buf_);
  std::string tok;
  while (std::getline(iss, tok, ',')) {
    try {
      uint32_t v = static_cast<uint32_t>(std::stoul(tok));
      if (v >= kOtbmVersionMin && v <= kOtbmVersionMax)
        otbm.push_back(v);
    } catch (...) {}
  }
  cv.setMapVersionsSupported(otbm);

  cv.setTransparent(transparent_bool_);
  cv.setExtended(extended_bool_);
  cv.setFrameDurations(frame_durations_bool_);
  cv.setFrameGroups(frame_groups_bool_);
  cv.setDefault(is_default_bool_);
}

void ClientPropertyEditor::applyDetectionResult(
    const Domain::ClientAssetDetectionResult &result) {
  if (result.dat_signature) {
    std::stringstream ss;
    ss << std::uppercase << std::hex << *result.dat_signature;
    std::strncpy(dat_sig_buf_, ss.str().c_str(), sizeof(dat_sig_buf_) - 1);
    property_states_[active_version_]["datSignature"] =
        Domain::PropertyVisualState::Saved;
  }

  if (result.spr_signature) {
    std::stringstream ss;
    ss << std::uppercase << std::hex << *result.spr_signature;
    std::strncpy(spr_sig_buf_, ss.str().c_str(), sizeof(spr_sig_buf_) - 1);
    property_states_[active_version_]["sprSignature"] =
        Domain::PropertyVisualState::Saved;
  }

  if (result.metadata_file_name) {
    std::strncpy(metadata_buf_, result.metadata_file_name->c_str(),
                 sizeof(metadata_buf_) - 1);
    property_states_[active_version_]["metadataFile"] =
        Domain::PropertyVisualState::Saved;
  }

  if (result.sprites_file_name) {
    std::strncpy(sprites_buf_, result.sprites_file_name->c_str(),
                 sizeof(sprites_buf_) - 1);
    property_states_[active_version_]["spritesFile"] =
        Domain::PropertyVisualState::Saved;
  }

  if (result.transparency) {
    transparent_bool_ = *result.transparency;
    property_states_[active_version_]["transparency"] =
        Domain::PropertyVisualState::Saved;
  }
  if (result.extended) {
    extended_bool_ = *result.extended;
    property_states_[active_version_]["extended"] =
        Domain::PropertyVisualState::Saved;
  }
  if (result.frame_durations) {
    frame_durations_bool_ = *result.frame_durations;
    property_states_[active_version_]["frameDurations"] =
        Domain::PropertyVisualState::Saved;
  }
  if (result.frame_groups) {
    frame_groups_bool_ = *result.frame_groups;
    property_states_[active_version_]["frameGroups"] =
        Domain::PropertyVisualState::Saved;
  }
}

void ClientPropertyEditor::render() {
  if (active_version_ == 0 || !registry_) {
    ImGui::TextDisabled("Select a client version to edit properties");
    return;
  }

  renderIdentitySection();
  ImGui::Spacing();
  renderFilesSection();
  ImGui::Spacing();
  renderCompatibilitySection();
  ImGui::Spacing();
  renderSignaturesSection();
  ImGui::Spacing();
  renderFeaturesSection();
}

void ClientPropertyEditor::renderStatusBar() {
  auto *cv = registry_->getVersion(active_version_);
  if (!cv)
    return;

  bool valid = cv->validateFiles();
  const char *statusIcon = valid ? ICON_FA_CIRCLE_CHECK : ICON_FA_TRIANGLE_EXCLAMATION;
  ImVec4 statusColor = valid ? kGreenStatus : ImVec4(0.9f, 0.4f, 0.3f, 1.0f);

  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
  ImGui::Text("%s", statusIcon);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  if (valid) {
    ImGui::Text("Client configuration is valid and files were found.");
  } else {
    ImGui::Text("Required files are missing. Check Paths and Files section.");
  }
}

void ClientPropertyEditor::renderIdentitySection() {
  if (!ImGui::TreeNodeEx("Identity", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Version:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(120);
  if (ImGui::InputInt("##version", &version_int_, 0, 0)) {
    version_int_ = std::max(0, version_int_);
    // Note: Registry doesn't support changing the primary key 'version' easily
    // We treat this as data only; renaming is handled by the version list
  }
  ImGui::PopItemWidth();

  ImGui::Text("Item Data Source:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  const char* sources[] = { "OTB (items.otb)", "SRV (items.srv)", "DAT only (No metadata file)" };
  if (ImGui::Combo("##datasource", &data_source_idx_, sources, IM_ARRAYSIZE(sources))) {
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
          cv->setDataSource(static_cast<::MapEditor::Domain::ItemDataSource>(data_source_idx_));
          cv->markDirty();
      }
  }
  if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("OTB: Modern mapping (Server ID -> Client ID)\n"
                        "SRV: Ancient text mapping (Server ID -> Client ID)\n"
                        "DAT only: No mapping file (Server ID = Client ID)");
  }
  ImGui::PopItemWidth();

  ImGui::Text("Display Name:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  if (ImGui::InputText("##name", name_buf_, sizeof(name_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setName(name_buf_);
      cv->markDirty();
    }
    if (on_change_)
      on_change_();
  }
  ImGui::PopItemWidth();

  ImGui::Text("Description:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  if (ImGui::InputText("##desc", desc_buf_, sizeof(desc_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setDescription(desc_buf_);
      cv->markDirty();
    }
  }
  ImGui::PopItemWidth();

  ImGui::TreePop();
}

void ClientPropertyEditor::renderFilesSection() {
  if (!ImGui::TreeNodeEx("Files && Paths", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Client Path:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
  if (ImGui::InputText("##clientpath", client_path_buf_, sizeof(client_path_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setClientPath(client_path_buf_);
      cv->markDirty();
    }
    if (on_change_)
      on_change_();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Folder containing Tibia.dat and Tibia.spr");
  ImGui::PopItemWidth();
  ImGui::SameLine();
  bool path_empty = (client_path_buf_[0] == '\0');
  if (path_empty) {
    float pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) + 1.0f) * 0.5f;
    ImVec4 yellow = ImVec4(1.0f, 0.8f, 0.2f, 0.5f + pulse * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, yellow);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
  }
  if (ImGui::Button(ICON_FA_FOLDER_OPEN "##browse", ImVec2(28, 0))) {
    NFD::UniquePath outPath;
    if (NFD::PickFolder(outPath) == NFD_OKAY) {
      std::strncpy(client_path_buf_, outPath.get(), sizeof(client_path_buf_) - 1);
      client_path_buf_[sizeof(client_path_buf_) - 1] = '\0';
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        cv->setClientPath(outPath.get());
        cv->markDirty();
      }
      if (on_change_)
        on_change_();
    }
  }
  if (path_empty)
    ImGui::PopStyleColor(2);

  ImGui::Text("Data Directory:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  if (ImGui::InputText("##datadir", data_dir_buf_, sizeof(data_dir_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setDataDirectory(data_dir_buf_);
      cv->markDirty();
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Editor data subdirectory for this client version");
  ImGui::PopItemWidth();

  renderAutoDetectedFileInputs();
  ImGui::TreePop();
}

void ClientPropertyEditor::renderAutoDetectedFileInputs() {
  const auto &states = property_states_[active_version_];

  auto render = [&](const char *label, const char *id, const char *prop, char *buf,
                    size_t bufsz, auto setter) {
    ImGui::Text("%s:", label);
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(-1);
    ScopedPropertyColor sc(prop, &states);
    if (ImGui::InputText(id, buf, bufsz)) {
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        setter(*cv);
        cv->markDirty();
      }
      if (isAutoProp(prop))
        property_states_[active_version_][prop] = Domain::PropertyVisualState::Pending;
    }
    ImGui::PopItemWidth();
  };

  render("Metadata File", "##metadata", "metadataFile", metadata_buf_,
         sizeof(metadata_buf_),
         [&](Domain::ClientVersion &cv) { cv.setMetadataFile(metadata_buf_); });
  render("Sprites File", "##sprites", "spritesFile", sprites_buf_,
         sizeof(sprites_buf_),
         [&](Domain::ClientVersion &cv) { cv.setSpritesFile(sprites_buf_); });

  auto *cv = registry_->getVersion(active_version_);
  if (cv && cv->getDataSource() != ::MapEditor::Domain::ItemDataSource::DAT) {
    const char *fileName = (cv->getDataSource() == ::MapEditor::Domain::ItemDataSource::SRV) ? "items.srv" : "items.otb";
    ImGui::Text("%s:", fileName);
    ImGui::SameLine(labelColumn());
    std::string otb_display;
    std::string cl_path(client_path_buf_);
    bool otb_found = false;
    if (!cl_path.empty()) {
      std::filesystem::path target = std::filesystem::path(cl_path) / fileName;
      std::filesystem::path fallback = std::filesystem::current_path() / "data" / fileName;
      if (std::filesystem::exists(target)) {
        otb_display = std::format("{} Found", ICON_FA_CHECK);
        otb_found = true;
      } else if (std::filesystem::exists(fallback)) {
        otb_display = std::format("{} Found in data/", ICON_FA_CHECK);
        otb_found = true;
      } else {
        otb_display = std::format("{} Not found", ICON_FA_XMARK);
      }
    } else {
      otb_display = std::format("{} Set client path first", ICON_FA_CIRCLE_QUESTION);
    }
    ImVec4 otb_col = otb_found ? kGreenStatus : ImVec4(0.9f, 0.4f, 0.3f, 1.0f);
    ImGui::TextColored(otb_col, "%s", otb_display.c_str());
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s is required to load maps with this configuration. Place it in the client folder or the editor's data directory.", fileName);
  }
}

void ClientPropertyEditor::renderCompatibilitySection() {
  if (!ImGui::TreeNodeEx("Compatibility", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("OTB ID:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(120);
  if (ImGui::InputInt("##otbid", &otb_id_int_)) {
    otb_id_int_ = std::max(0, otb_id_int_);
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->markDirty();
      cv->setOtbVersion(static_cast<uint32_t>(otb_id_int_));
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("OTB Major:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(120);
  if (ImGui::InputInt("##otbmajor", &otb_major_int_)) {
    otb_major_int_ = std::max(0, otb_major_int_);
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->markDirty();
      cv->setOtbMajor(static_cast<uint32_t>(otb_major_int_));
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("OTBM Versions:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  if (ImGui::InputText("##otbmver", otbm_versions_buf_, sizeof(otbm_versions_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->markDirty();
      std::vector<uint32_t> otbm;
      std::istringstream iss(otbm_versions_buf_);
      std::string tok;
      while (std::getline(iss, tok, ',')) {
        try {
          uint32_t v = static_cast<uint32_t>(std::stoul(tok));
          if (v >= kOtbmVersionMin && v <= kOtbmVersionMax)
            otbm.push_back(v);
        } catch (...) {}
      }
      cv->setMapVersionsSupported(otbm);
    }
  }
  ImGui::PopItemWidth();

  ImGui::TreePop();
}

void ClientPropertyEditor::renderSignaturesSection() {
  if (!ImGui::TreeNodeEx("Signatures", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const auto &states = property_states_[active_version_];

  ImGui::Text("DAT Signature:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  {
    ScopedPropertyColor sc("datSignature", &states);
    if (ImGui::InputText("##datsig", dat_sig_buf_, sizeof(dat_sig_buf_),
                         ImGuiInputTextFlags_CharsHexadecimal)) {
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        uint32_t sig = 0;
        std::istringstream(dat_sig_buf_) >> std::hex >> sig;
        cv->setDatSignature(sig);
        cv->markDirty();
        if (isAutoProp("datSignature"))
          property_states_[active_version_]["datSignature"] =
              Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("SPR Signature:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  {
    ScopedPropertyColor sc("sprSignature", &states);
    if (ImGui::InputText("##sprsig", spr_sig_buf_, sizeof(spr_sig_buf_),
                         ImGuiInputTextFlags_CharsHexadecimal)) {
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        uint32_t sig = 0;
        std::istringstream(spr_sig_buf_) >> std::hex >> sig;
        cv->setSprSignature(sig);
        cv->markDirty();
        if (isAutoProp("sprSignature"))
          property_states_[active_version_]["sprSignature"] =
              Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::TreePop();
}

void ClientPropertyEditor::renderFeaturesSection() {
  if (!ImGui::TreeNodeEx("Features", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const auto &states = property_states_[active_version_];

  float half = ImGui::GetContentRegionAvail().x * 0.48f;

  auto boolBox = [&](const char *label, const char *prop, bool &val, auto setter) {
    ScopedPropertyColor sc(prop, &states);
    if (ImGui::Checkbox(label, &val)) {
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        setter(cv, val);
        cv->markDirty();
        if (isAutoProp(prop))
          property_states_[active_version_][prop] = Domain::PropertyVisualState::Pending;
      }
    }
  };

  boolBox("Transparency", "transparency", transparent_bool_,
          [](Domain::ClientVersion *c, bool v) { c->setTransparent(v); });
  ImGui::SameLine(0, 20);
  ImGui::SetCursorPosX(labelColumn() + half);
  boolBox("Extended", "extended", extended_bool_,
          [](Domain::ClientVersion *c, bool v) { c->setExtended(v); });

  boolBox("Frame Durations", "frameDurations", frame_durations_bool_,
          [](Domain::ClientVersion *c, bool v) { c->setFrameDurations(v); });
  ImGui::SameLine(0, 20);
  ImGui::SetCursorPosX(labelColumn() + half);
  boolBox("Frame Groups", "frameGroups", frame_groups_bool_,
          [](Domain::ClientVersion *c, bool v) { c->setFrameGroups(v); });

  ScopedPropertyColor sc("default", &states);
  if (ImGui::Checkbox("Set as Default", &is_default_bool_)) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setDefault(is_default_bool_);
      if (is_default_bool_ && registry_)
        registry_->setDefaultVersion(cv->getVersion());
      cv->markDirty();
    }
  }

  ImGui::TreePop();
}

} // namespace UI
} // namespace MapEditor
