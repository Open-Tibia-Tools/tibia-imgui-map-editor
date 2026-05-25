#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <nfd.hpp>
#include <sstream>

namespace MapEditor {
namespace UI {

namespace {

const char *getParserName(uint32_t version) {
  if (version >= 1057) return "DAT Parser 10.57+";
  if (version >= 1050) return "DAT Parser 10.50+";
  if (version >= 1010) return "DAT Parser 10.10+";
  if (version >= 960) return "DAT Parser 9.60+";
  if (version >= 860) return "DAT Parser 8.60";
  if (version >= 780) return "DAT Parser 7.80";
  if (version >= 755) return "DAT Parser 7.55";
  if (version >= 740) return "DAT Parser 7.40";
  if (version >= 710) return "DAT Parser 7.10";
  return "Unknown Parser";
}

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
    if (!states_)
      return;
    auto it = states_->find(name_);
    if (it == states_->end())
      return;
    auto s = it->second;
    if (s == Domain::PropertyVisualState::Default)
      return;
    const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    ImVec4 tint = ImVec4(1, 1, 1, 0);
    if (s == Domain::PropertyVisualState::Pending)
      tint = kYellow;
    else if (s == Domain::PropertyVisualState::Undetected)
      tint = kRed;
    else if (s == Domain::PropertyVisualState::Saved)
      tint = kGreen;
    else
      return;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, blend(bg, tint, 0.30f));
    pushed_ = true;
  }

  ~ScopedPropertyColor() {
    if (pushed_)
      ImGui::PopStyleColor();
  }

private:
  const std::unordered_map<std::string, Domain::PropertyVisualState> *states_ = nullptr;
  const char *name_ = nullptr;
  bool pushed_ = false;
};

float labelColumn() { return 195.0f; }

} // namespace

void ClientPropertyEditor::setPropertyState(uint32_t version, const char *name,
                                             Domain::PropertyVisualState state) {
  property_states_[version][name] = state;
}

Domain::PropertyVisualState
ClientPropertyEditor::getPropertyState(uint32_t version, const char *name) const {
  auto it = property_states_.find(version);
  if (it == property_states_.end())
    return Domain::PropertyVisualState::Default;
  auto pit = it->second.find(name);
  if (pit == it->second.end())
    return Domain::PropertyVisualState::Default;
  return pit->second;
}

void ClientPropertyEditor::clearPropertyStates(uint32_t version) {
  property_states_.erase(version);
}

void ClientPropertyEditor::markAllPendingAsSaved(uint32_t version) {
  auto it = property_states_.find(version);
  if (it == property_states_.end())
    return;
  for (auto &[name, state] : it->second) {
    if (state == Domain::PropertyVisualState::Pending)
      state = Domain::PropertyVisualState::Saved;
  }
}

void ClientPropertyEditor::syncFromClient(const Domain::ClientVersion &cv) {
  version_int_ = static_cast<int>(cv.getVersion());
  std::strncpy(name_buf_, cv.getName().c_str(), sizeof(name_buf_) - 1);
  name_buf_[sizeof(name_buf_) - 1] = '\0';
  std::strncpy(desc_buf_, cv.getDescription().c_str(), sizeof(desc_buf_) - 1);
  desc_buf_[sizeof(desc_buf_) - 1] = '\0';
  std::strncpy(data_dir_buf_, cv.getDataDirectory().c_str(), sizeof(data_dir_buf_) - 1);
  data_dir_buf_[sizeof(data_dir_buf_) - 1] = '\0';
  std::strncpy(client_path_buf_, cv.getClientPath().string().c_str(),
               sizeof(client_path_buf_) - 1);
  client_path_buf_[sizeof(client_path_buf_) - 1] = '\0';
  std::strncpy(metadata_buf_, (cv.getClientPath() / cv.getMetadataFile()).string().c_str(), sizeof(metadata_buf_) - 1);
  metadata_buf_[sizeof(metadata_buf_) - 1] = '\0';
  std::strncpy(sprites_buf_, (cv.getClientPath() / cv.getSpritesFile()).string().c_str(), sizeof(sprites_buf_) - 1);
  sprites_buf_[sizeof(sprites_buf_) - 1] = '\0';
  std::strncpy(items_db_buf_, cv.getCustomItemsDbPath().string().c_str(),
               sizeof(items_db_buf_) - 1);
  items_db_buf_[sizeof(items_db_buf_) - 1] = '\0';

  std::snprintf(dat_sig_buf_, sizeof(dat_sig_buf_), "%08X", cv.getDatSignature());
  std::snprintf(spr_sig_buf_, sizeof(spr_sig_buf_), "%08X", cv.getSprSignature());

  data_source_idx_ = static_cast<int>(cv.getDataSource());

  otb_id_int_ = static_cast<int>(cv.getOtbVersion());
  otb_major_int_ = static_cast<int>(cv.getOtbMajor());

  std::string otbm;
  for (size_t i = 0; i < cv.getMapVersionsSupported().size(); ++i) {
    if (i > 0)
      otbm += ", ";
    otbm += std::to_string(cv.getMapVersionsSupported()[i]);
  }
  std::strncpy(otbm_versions_buf_, otbm.c_str(), sizeof(otbm_versions_buf_) - 1);
  otbm_versions_buf_[sizeof(otbm_versions_buf_) - 1] = '\0';

  transparent_bool_ = cv.isTransparent();
  extended_bool_ = cv.isExtended();
  frame_durations_bool_ = cv.hasFrameDurations();
  frame_groups_bool_ = cv.hasFrameGroups();
  is_default_bool_ = cv.isDefault();
}

void ClientPropertyEditor::syncToClient(Domain::ClientVersion &cv) {
  uint32_t new_version = static_cast<uint32_t>(std::max(0, version_int_));

  cv.setName(name_buf_);
  cv.setDescription(desc_buf_);
  cv.setDataDirectory(data_dir_buf_);
  cv.setClientPath(client_path_buf_);
  cv.setMetadataFile(std::filesystem::path(metadata_buf_).filename().string());
  cv.setSpritesFile(std::filesystem::path(sprites_buf_).filename().string());
  cv.setOtbMajor(static_cast<uint32_t>(std::max(0, otb_major_int_)));

  std::vector<uint32_t> otbm;
  std::string str(otbm_versions_buf_);
  std::istringstream iss(str);
  std::string tok;
  while (std::getline(iss, tok, ',')) {
    try {
      uint32_t v = static_cast<uint32_t>(std::stoul(tok));
      if (v >= 1 && v <= 4)
        otbm.push_back(v);
    } catch (...) {
    }
  }
  cv.setMapVersionsSupported(otbm);

  cv.setTransparent(transparent_bool_);
  cv.setExtended(extended_bool_);
  cv.setFrameDurations(frame_durations_bool_);
  cv.setFrameGroups(frame_groups_bool_);

  if (is_default_bool_ && registry_)
    registry_->setDefaultVersion(cv.getVersion());

  uint32_t ds = 0, ss = 0;
  std::istringstream(dat_sig_buf_) >> std::hex >> ds;
  std::istringstream(spr_sig_buf_) >> std::hex >> ss;
  cv.setDatSignature(ds);
  cv.setSprSignature(ss);

  if (cv.getVersion() != new_version)
    cv.setVersion(new_version);

  cv.setDataSource(static_cast<::MapEditor::Domain::ItemDataSource>(
      std::clamp(data_source_idx_, 0, static_cast<int>(Domain::ItemDataSource::DAT))));

  if (items_db_buf_[0] != '\0')
    cv.setCustomItemsDbPath(std::filesystem::path(items_db_buf_));
  else
    cv.setCustomItemsDbPath(std::filesystem::path());

  cv.markDirty();
}

void ClientPropertyEditor::applyDetectionResult(
    const Domain::ClientAssetDetectionResult &result) {
  auto &states = property_states_[active_version_];
  auto apply = [&](const char *name, bool detected) {
    states[name] = detected ? Domain::PropertyVisualState::Pending
                            : Domain::PropertyVisualState::Undetected;
  };

  if (result.metadata_file_name) {
    std::strncpy(metadata_buf_, result.metadata_file_name->c_str(), sizeof(metadata_buf_) - 1);
    metadata_buf_[sizeof(metadata_buf_) - 1] = '\0';
  }
  apply("metadataFile", result.metadata_file_name.has_value());

  if (result.sprites_file_name) {
    std::strncpy(sprites_buf_, result.sprites_file_name->c_str(), sizeof(sprites_buf_) - 1);
    sprites_buf_[sizeof(sprites_buf_) - 1] = '\0';
  }
  apply("spritesFile", result.sprites_file_name.has_value());

  if (result.dat_signature)
    std::snprintf(dat_sig_buf_, sizeof(dat_sig_buf_), "%08X", *result.dat_signature);
  apply("datSignature", result.dat_signature.has_value());

  if (result.spr_signature)
    std::snprintf(spr_sig_buf_, sizeof(spr_sig_buf_), "%08X", *result.spr_signature);
  apply("sprSignature", result.spr_signature.has_value());

  extended_bool_ = false;
  transparent_bool_ = false;
  frame_durations_bool_ = false;
  frame_groups_bool_ = false;

  if (result.transparency)
    transparent_bool_ = *result.transparency;
  apply("transparency", result.transparency.has_value());

  if (result.extended)
    extended_bool_ = *result.extended;
  apply("extended", result.extended.has_value());

  if (result.frame_durations)
    frame_durations_bool_ = *result.frame_durations;
  apply("frameDurations", result.frame_durations.has_value());

  if (result.frame_groups)
    frame_groups_bool_ = *result.frame_groups;
  apply("frameGroups", result.frame_groups.has_value());
}

void ClientPropertyEditor::render() {
  if (active_version_ == 0 || !registry_)
    return;
  auto *cv = registry_->getVersion(active_version_);
  if (!cv)
    return;

  renderIdentitySection();
  renderFilesSection();
  renderCompatibilitySection();
  renderSignaturesSection();
  renderFeaturesSection();
}

void ClientPropertyEditor::renderStatusBar() {
  if (!registry_)
    return;
  auto *cv = registry_->getVersion(active_version_);
  if (!cv)
    return;

  ImGui::Spacing();
  ImGui::Separator();

  ImGui::SetCursorPosX(12);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);

  bool dirty = cv->isDirty();
  ImU32 dot_color = dirty ? IM_COL32(231, 209, 46, 255) : IM_COL32(110, 209, 110, 255);
  ImGui::GetWindowDrawList()->AddCircleFilled(
      ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 7), 5,
      dot_color);

  ImGui::SetCursorPosX(24);
  ImGui::TextColored(kTextMuted, "Status: ");
  ImGui::SameLine(0, 0);
  ImVec4 status_col = dirty ? ImVec4(0.9f, 0.7f, 0.2f, 1.0f) : kGreenStatus;
  ImGui::TextColored(status_col, "%s", cv->getName().c_str());
  ImGui::SameLine(0, 0);
  ImGui::TextColored(kTextMuted, "  |  ");
  ImGui::SameLine(0, 0);
  ImGui::TextColored(status_col, "%s", dirty ? "Modified" : "Saved");
}

void ClientPropertyEditor::renderIdentitySection() {
  if (!ImGui::TreeNodeEx("Identity", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  auto *cv = registry_->getVersion(active_version_);
  const auto &states = property_states_[active_version_];

  ImGui::Text("Item Data Source:");
  ImGui::SameLine(labelColumn());

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));
  bool is_otb = (data_source_idx_ == 0);
  bool is_srv = (data_source_idx_ == 1);
  bool is_dat = (data_source_idx_ == 2);

  if (is_otb) {
    ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
  }
  if (ImGui::Button("OTB", ImVec2(42, 0))) {
    data_source_idx_ = 0;
    if (cv) {
      cv->setDataSource(Domain::ItemDataSource::OTB);
      cv->markDirty();
    }
  }
  if (is_otb) ImGui::PopStyleColor(2);

  ImGui::SameLine();
  if (is_srv) {
    ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
  }
  if (ImGui::Button("SRV", ImVec2(42, 0))) {
    data_source_idx_ = 1;
    if (cv) {
      cv->setDataSource(Domain::ItemDataSource::SRV);
      cv->markDirty();
    }
  }
  if (is_srv) ImGui::PopStyleColor(2);

  ImGui::SameLine();
  if (is_dat) {
    ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
  }
  if (ImGui::Button("DAT", ImVec2(42, 0))) {
    data_source_idx_ = 2;
    if (cv) {
      cv->setDataSource(Domain::ItemDataSource::DAT);
      cv->markDirty();
    }
  }
  if (is_dat) ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();

  ImGui::Text("Client Version:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(140);
  std::vector<std::string> ver_labels;
  std::vector<uint32_t> ver_values;
  if (registry_) {
    for (const auto &[ver, cv2] : registry_->getVersionsMap()) {
      ver_labels.push_back(std::format("{} ({})", cv2.getName(), ver));
      ver_values.push_back(ver);
    }
  }
  int ver_sel = -1;
  for (size_t i = 0; i < ver_values.size(); ++i) {
    if (static_cast<int>(ver_values[i]) == version_int_) {
      ver_sel = static_cast<int>(i);
      break;
    }
  }
  if (ImGui::BeginCombo("##versioncombo", ver_sel >= 0 ? ver_labels[ver_sel].c_str() : "")) {
    for (size_t i = 0; i < ver_values.size(); ++i) {
      bool is_selected = (ver_sel == static_cast<int>(i));
      if (ImGui::Selectable(ver_labels[i].c_str(), is_selected)) {
        ver_sel = static_cast<int>(i);
        version_int_ = static_cast<int>(ver_values[ver_sel]);
        if (cv) {
          cv->setVersion(static_cast<uint32_t>(version_int_));
          std::string new_data_dir = std::format("data/{}", version_int_);
          cv->setDataDirectory(new_data_dir);
          std::strncpy(data_dir_buf_, new_data_dir.c_str(), sizeof(data_dir_buf_) - 1);
          data_dir_buf_[sizeof(data_dir_buf_) - 1] = '\0';
          cv->markDirty();
        }
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  ImGui::Text("Parser:");
  ImGui::SameLine();
  ImGui::TextColored(kGreenStatus, "%s", getParserName(static_cast<uint32_t>(version_int_)));

  ImGui::Text("Name:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  {
    ScopedPropertyColor sc("name", &states);
    if (ImGui::InputText("##name", name_buf_, sizeof(name_buf_))) {
      auto *cver = registry_->getVersion(active_version_);
      if (cver) {
        cver->setName(name_buf_);
        cver->markDirty();
        property_states_[active_version_]["name"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("Description:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  {
    ScopedPropertyColor sc("desc", &states);
    if (ImGui::InputText("##desc", desc_buf_, sizeof(desc_buf_))) {
      auto *cver = registry_->getVersion(active_version_);
      if (cver) {
        cver->setDescription(desc_buf_);
        cver->markDirty();
        property_states_[active_version_]["desc"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::TreePop();
}

void ClientPropertyEditor::renderFilesSection() {
  if (!ImGui::TreeNodeEx("Files && Paths", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  auto *cv = registry_->getVersion(active_version_);
  const auto &states = property_states_[active_version_];

  // Client Path
  ImGui::Text("Client Path:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
  {
    ScopedPropertyColor sc("clientPath", &states);
    if (ImGui::InputText("##clientpath", client_path_buf_, sizeof(client_path_buf_))) {
      if (cv) {
        cv->setClientPath(client_path_buf_);
        cv->markDirty();
        property_states_[active_version_]["clientPath"] = Domain::PropertyVisualState::Pending;
      }
      if (on_change_)
        on_change_();
    }
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

  // Data Directory
  ImGui::Text("Data Directory:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
  {
    ScopedPropertyColor sc("dataDir", &states);
    if (ImGui::InputText("##datadir", data_dir_buf_, sizeof(data_dir_buf_))) {
      if (cv) {
        cv->setDataDirectory(data_dir_buf_);
        cv->markDirty();
        property_states_[active_version_]["dataDir"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Editor data subdirectory for this client version. Defaults to data/<version>");
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN "##ddbrowse", ImVec2(28, 0))) {
    NFD::UniquePath outPath;
    if (NFD::PickFolder(outPath) == NFD_OKAY) {
      std::strncpy(data_dir_buf_, outPath.get(), sizeof(data_dir_buf_) - 1);
      data_dir_buf_[sizeof(data_dir_buf_) - 1] = '\0';
      if (cv) {
        cv->setDataDirectory(outPath.get());
        cv->markDirty();
        property_states_[active_version_]["dataDir"] = Domain::PropertyVisualState::Pending;
      }
    }
  }

  // Metadata File (editable input, auto-filled from client path)
  ImGui::Text("Metadata File:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
  {
    ScopedPropertyColor sc("metadata", &states);
    if (ImGui::InputText("##metadata", metadata_buf_, sizeof(metadata_buf_))) {
      if (cv) {
        cv->setMetadataFile(std::filesystem::path(metadata_buf_).filename().string());
        cv->markDirty();
        property_states_[active_version_]["metadata"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("File name of the DAT (.dat) file");
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN "##metabrowse", ImVec2(28, 0))) {
    NFD::UniquePath outPath;
    if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
      std::filesystem::path selectedPath(outPath.get());
      std::strncpy(metadata_buf_, selectedPath.string().c_str(), sizeof(metadata_buf_) - 1);
      metadata_buf_[sizeof(metadata_buf_) - 1] = '\0';
      if (cv) {
        auto parent = selectedPath.parent_path();
        if (!parent.empty() && parent != cv->getClientPath()) {
          cv->setClientPath(parent);
          std::strncpy(client_path_buf_, parent.string().c_str(), sizeof(client_path_buf_) - 1);
          client_path_buf_[sizeof(client_path_buf_) - 1] = '\0';
        }
        cv->setMetadataFile(selectedPath.filename().string());
        cv->markDirty();
        property_states_[active_version_]["metadata"] = Domain::PropertyVisualState::Pending;
      }
      if (on_change_)
        on_change_();
    }
  }

  // Sprites File (editable input, auto-filled from client path)
  ImGui::Text("Sprites File:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
  {
    ScopedPropertyColor sc("sprites", &states);
    if (ImGui::InputText("##sprites", sprites_buf_, sizeof(sprites_buf_))) {
      if (cv) {
        cv->setSpritesFile(std::filesystem::path(sprites_buf_).filename().string());
        cv->markDirty();
        property_states_[active_version_]["sprites"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("File name of the SPR (.spr) file");
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN "##sprbrowse", ImVec2(28, 0))) {
    NFD::UniquePath outPath;
    if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
      std::filesystem::path selectedPath(outPath.get());
      std::strncpy(sprites_buf_, selectedPath.string().c_str(), sizeof(sprites_buf_) - 1);
      sprites_buf_[sizeof(sprites_buf_) - 1] = '\0';
      if (cv) {
        auto parent = selectedPath.parent_path();
        if (!parent.empty() && parent != cv->getClientPath()) {
          cv->setClientPath(parent);
          std::strncpy(client_path_buf_, parent.string().c_str(), sizeof(client_path_buf_) - 1);
          client_path_buf_[sizeof(client_path_buf_) - 1] = '\0';
        }
        cv->setSpritesFile(selectedPath.filename().string());
        cv->markDirty();
        property_states_[active_version_]["sprites"] = Domain::PropertyVisualState::Pending;
      }
      if (on_change_)
        on_change_();
    }
  }

  // Items Database
  {
    ImGui::Text("Items Database:");
    ImGui::SameLine(labelColumn());
    bool is_dat_source = (data_source_idx_ == 2);
    if (is_dat_source)
      ImGui::BeginDisabled();
    std::string items_path;
    if (cv) items_path = cv->getItemMetadataPath().string();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
    ImGui::InputText("##itemsdb", items_path.data(), items_path.size(),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && !items_path.empty())
      ImGui::SetTooltip("%s", items_path.c_str());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##itemsdbbrowse", ImVec2(28, 0))) {
      NFD::UniquePath outPath;
      if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
        std::string selected(outPath.get());
        std::strncpy(items_db_buf_, selected.c_str(), sizeof(items_db_buf_) - 1);
        items_db_buf_[sizeof(items_db_buf_) - 1] = '\0';
        if (cv) {
          cv->setCustomItemsDbPath(std::filesystem::path(selected));
          cv->markDirty();
        }
      }
    }
    if (is_dat_source)
      ImGui::EndDisabled();
  }

  ImGui::TreePop();
}

void ClientPropertyEditor::renderCompatibilitySection() {
  if (!ImGui::TreeNodeEx("Compatibility", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const auto &states = property_states_[active_version_];

  char otb_id_buf[16], otb_major_buf[16];
  std::snprintf(otb_id_buf, sizeof(otb_id_buf), "%d", otb_id_int_);
  std::snprintf(otb_major_buf, sizeof(otb_major_buf), "%d", otb_major_int_);

  ImGui::Text("OTB ID:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(80);
  {
    ScopedPropertyColor sc("otbId", &states);
    if (ImGui::InputText("##otbid", otb_id_buf, sizeof(otb_id_buf),
                         ImGuiInputTextFlags_CharsDecimal)) {
      otb_id_int_ = std::max(0, std::atoi(otb_id_buf));
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        cv->markDirty();
        cv->setOtbVersion(static_cast<uint32_t>(otb_id_int_));
        property_states_[active_version_]["otbId"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("Major:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(80);
  {
    ScopedPropertyColor sc("otbMajor", &states);
    if (ImGui::InputText("##otbmajor", otb_major_buf, sizeof(otb_major_buf),
                         ImGuiInputTextFlags_CharsDecimal)) {
      otb_major_int_ = std::max(0, std::atoi(otb_major_buf));
      auto *cv = registry_->getVersion(active_version_);
      if (cv) {
        cv->markDirty();
        cv->setOtbMajor(static_cast<uint32_t>(otb_major_int_));
        property_states_[active_version_]["otbMajor"] = Domain::PropertyVisualState::Pending;
      }
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("OTBM:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(80);
  {
    ScopedPropertyColor sc("otbmVersions", &states);
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
        property_states_[active_version_]["otbmVersions"] = Domain::PropertyVisualState::Pending;
      }
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
