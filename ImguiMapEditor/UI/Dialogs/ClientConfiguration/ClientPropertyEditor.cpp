#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <format>
#include <imgui.h>
#include <nfd.hpp>
#include <sstream>

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
  std::strncpy(metadata_buf_, cv.getMetadataFile().c_str(), sizeof(metadata_buf_) - 1);
  metadata_buf_[sizeof(metadata_buf_) - 1] = '\0';
  std::strncpy(sprites_buf_, cv.getSpritesFile().c_str(), sizeof(sprites_buf_) - 1);
  sprites_buf_[sizeof(sprites_buf_) - 1] = '\0';

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
  cv.setMetadataFile(metadata_buf_);
  cv.setSpritesFile(sprites_buf_);
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

  ImGui::Text("Item Data Source:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(200);
  const char *sources[] = {"Tibia.dat + Tibia.spr + items.otb", "Tibia.dat + Tibia.spr + items.srv", "Tibia.dat + Tibia.spr only (Client IDs)"};
  if (ImGui::Combo("##datasource", &data_source_idx_, sources, IM_ARRAYSIZE(sources))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setDataSource(static_cast<::MapEditor::Domain::ItemDataSource>(
          std::clamp(data_source_idx_, 0, static_cast<int>(IM_ARRAYSIZE(sources) - 1))));
      cv->markDirty();
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("Version:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(120);
  if (ImGui::InputInt("##version", &version_int_)) {
    version_int_ = std::max(0, version_int_);
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setVersion(static_cast<uint32_t>(version_int_));
      cv->markDirty();
    }
  }
  ImGui::PopItemWidth();

  ImGui::Text("Name:");
  ImGui::SameLine(labelColumn());
  ImGui::PushItemWidth(-1);
  if (ImGui::InputText("##name", name_buf_, sizeof(name_buf_))) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->setName(name_buf_);
      cv->markDirty();
    }
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
  if (cv && cv->getDataSource() != Domain::ItemDataSource::DAT) {
    const char *fileName = (cv->getDataSource() == Domain::ItemDataSource::SRV) ? "items.srv" : "items.otb";
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
