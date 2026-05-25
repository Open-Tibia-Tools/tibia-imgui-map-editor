#include "UI/Dialogs/ClientConfiguration/ClientConfigurationDialog.h"
#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"
#include "Domain/ClientVersion.h"
#include "Services/ClientAssetDetector.h"
#include "Services/ClientVersionPersistence.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <imgui.h>
#include <map>
#include <nfd.hpp>
#include <set>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace UI {

namespace {

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

void pushCompactStyle() {
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6.0f, 4.0f));
}

void popCompactStyle() { ImGui::PopStyleVar(5); }

constexpr ImVec4 kBlueAccent = ImVec4(0.19f, 0.44f, 0.84f, 1.0f);
constexpr ImVec4 kBlueHover = ImVec4(0.25f, 0.50f, 0.92f, 1.0f);
constexpr ImVec4 kBlueActive = ImVec4(0.15f, 0.38f, 0.76f, 1.0f);
constexpr ImVec4 kRedDelete = ImVec4(0.78f, 0.14f, 0.20f, 1.0f);
constexpr ImVec4 kRedHover = ImVec4(0.86f, 0.20f, 0.27f, 1.0f);
constexpr ImVec4 kGreenStatus = ImVec4(0.43f, 0.82f, 0.43f, 1.0f);
constexpr ImVec4 kTextOffWhite = ImVec4(0.85f, 0.87f, 0.91f, 1.0f);
constexpr ImVec4 kTextMuted = ImVec4(0.67f, 0.70f, 0.75f, 1.0f);
constexpr ImVec4 kBgDark = ImVec4(0.12f, 0.13f, 0.15f, 0.94f);

} // namespace

ClientConfigurationDialog::ClientConfigurationDialog()
    : editor_(std::make_unique<ClientPropertyEditor>()) {}

ClientConfigurationDialog::~ClientConfigurationDialog() = default;

void ClientConfigurationDialog::open(Services::ClientVersionRegistry &registry,
                                     Services::ConfigService &config) {
  registry_ = &registry;
  config_ = &config;
  is_open_ = true;
  active_version_ = 0;
  active_tab_ = 0;
  search_buf_[0] = '\0';
  search_filter_.clear();
  pending_deleted_.clear();

  editor_->setRegistry(registry_);
  editor_->setChangeCallback([this]() { runAssetDetection(); });

  if (registry_->getDefaultVersion() > 0) {
    active_version_ = registry_->getDefaultVersion();
  } else if (!registry_->getVersionsMap().empty()) {
    active_version_ = registry_->getVersionsMap().begin()->first;
  }

  version_groups_.clear();
  std::map<int, std::vector<uint32_t>> grouped;
  for (const auto &[ver_num, cv] : registry_->getVersionsMap()) {
    grouped[getMajorGroup(ver_num)].push_back(ver_num);
  }
  for (auto &[major, vers] : grouped) {
    std::sort(vers.begin(), vers.end());
    std::string label;
    if (major >= 1000)
      label = std::format("{}.{}", major, 0);
    else if (major >= 100)
      label = std::format("{}.x", major);
    else
      label = std::format("{}.x", major);
    version_groups_.push_back({major, label, true, std::move(vers)});
  }
  std::sort(version_groups_.begin(), version_groups_.end(),
            [](const VersionGroup &a, const VersionGroup &b) {
              return a.major < b.major;
            });

  populateVersionData();


  int def_group = getMajorGroup(active_version_);
  for (size_t i = 0; i < version_groups_.size(); ++i) {
    if (version_groups_[i].major == def_group) {
      active_tab_ = static_cast<int>(i);
      break;
    }
  }

  if (active_version_ != 0) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      cv->backup();
      editor_->syncFromClient(*cv);
      editor_->setActiveVersion(active_version_);
    }
  }
}

void ClientConfigurationDialog::close() { is_open_ = false; }

int ClientConfigurationDialog::getMajorGroup(uint32_t version) const {
  if (version >= 10000)
    return static_cast<int>(version) / 1000;
  if (version >= 100)
    return static_cast<int>(version) / 100;
  return static_cast<int>(version) / 10;
}

bool ClientConfigurationDialog::matchesFilter(const Domain::ClientVersion &ver) const {
  if (search_filter_.empty())
    return true;
  std::string haystack = toLower(ver.getName()) + " " + toLower(ver.getDescription()) +
                         " " + std::to_string(ver.getVersion()) + " " +
                         std::to_string(ver.getOtbVersion());
  return haystack.find(search_filter_) != std::string::npos;
}

void ClientConfigurationDialog::populateVersionData() {
  if (version_groups_.empty())
    return;

  // Gather all existing group majors
  std::set<int> existing_groups;
  for (const auto &grp : version_groups_)
    existing_groups.insert(grp.major);

  // Clear version vectors
  for (auto &grp : version_groups_)
    grp.versions.clear();

  // Fill and track new groups
  std::map<int, std::vector<uint32_t>> all_by_major;
  for (const auto &[ver_num, cv] : registry_->getVersionsMap()) {
    if (pending_deleted_.count(ver_num))
      continue;
    if (!matchesFilter(cv))
      continue;
    int grp_idx = getMajorGroup(ver_num);
    all_by_major[grp_idx].push_back(ver_num);
  }

  // Add new groups
  for (const auto &[major, vers] : all_by_major) {
    if (existing_groups.count(major))
      continue;
    std::string label;
    if (major >= 1000)
      label = std::format("{}.{}", major, 0);
    else if (major >= 100)
      label = std::format("{}.x", major);
    else
      label = std::format("{}.x", major);
    version_groups_.push_back({major, label, true, vers});
  }

  // Sort groups
  std::sort(version_groups_.begin(), version_groups_.end(),
            [](const VersionGroup &a, const VersionGroup &b) {
              return a.major < b.major;
            });

  // Fill existing groups
  for (auto &grp : version_groups_) {
    auto it = all_by_major.find(grp.major);
    if (it != all_by_major.end()) {
      grp.versions = std::move(it->second);
      std::sort(grp.versions.begin(), grp.versions.end());
    }
  }

  if (!version_groups_.empty()) {
    if (active_tab_ >= static_cast<int>(version_groups_.size()))
      active_tab_ = 0;
    // Build merged filtered list from visible groups for backward compat
    filtered_versions_.clear();
    for (const auto &grp : version_groups_) {
      if (!grp.visible) continue;
      for (auto v : grp.versions)
        filtered_versions_.push_back(v);
    }
  } else {
    filtered_versions_.clear();
    active_tab_ = 0;
  }
}

void ClientConfigurationDialog::selectClient(uint32_t version) {
  if (version == active_version_)
    return;
  active_version_ = version;
  if (version == 0)
    return;

  auto *cv = registry_->getVersion(version);
  if (cv) {
    cv->backup();
    editor_->syncFromClient(*cv);
    editor_->setActiveVersion(version);
  }
}

void ClientConfigurationDialog::runAssetDetection() {
  if (active_version_ == 0 || !registry_)
    return;
  auto *cv = registry_->getVersion(active_version_);
  if (!cv)
    return;
  editor_->syncToClient(*cv);

  auto result = Services::ClientAssetDetector::detect(
      cv->getClientPath(), cv->getMetadataFile(), cv->getSpritesFile(),
      &registry_->getVersionsMap());

  editor_->applyDetectionResult(result);
  editor_->syncToClient(*cv);
  for (const auto &w : result.warnings)
    spdlog::warn("Asset detection: {}", w);
}

void ClientConfigurationDialog::addClient() {
  std::string new_name = "New Client";
  int counter = 1;
  for (bool unique = false; !unique;) {
    unique = true;
    for (const auto &[num, ver] : registry_->getVersionsMap()) {
      if (ver.getName() == new_name) {
        new_name = "New Client " + std::to_string(counter++);
        unique = false;
        break;
      }
    }
  }

  uint32_t new_version = 99999;
  while (registry_->getVersionsMap().count(new_version))
    ++new_version;

  Domain::ClientVersion cv(new_version, new_name, 0);
  cv.setDataDirectory(std::format("data/{}", new_version));
  cv.setMetadataFile("Tibia.dat");
  cv.setSpritesFile("Tibia.spr");
  cv.setOtbMajor(3);
  cv.markDirty();

  registry_->addClient(cv);
  registry_->backupVersion(new_version);
  populateVersionData();
  selectClient(new_version);
}

void ClientConfigurationDialog::duplicateClient() {
  if (active_version_ == 0)
    return;
  auto *src = registry_->getVersion(active_version_);
  if (!src)
    return;

  std::string new_name = src->getName() + " (Copy)";
  uint32_t new_version = src->getVersion();
  while (registry_->getVersionsMap().count(new_version))
    ++new_version;

  Domain::ClientVersion clone(new_version, new_name, src->getOtbVersion());
  clone.setDescription(src->getDescription());
  clone.setDataDirectory(src->getDataDirectory());
  clone.setClientPath(src->getClientPath());
  clone.setOtbMajor(src->getOtbMajor());
  clone.setMapVersionsSupported(src->getMapVersionsSupported());
  clone.setDatSignature(src->getDatSignature());
  clone.setSprSignature(src->getSprSignature());
  clone.setMetadataFile(src->getMetadataFile());
  clone.setSpritesFile(src->getSpritesFile());
  clone.setTransparent(src->isTransparent());
  clone.setExtended(src->isExtended());
  clone.setFrameDurations(src->hasFrameDurations());
  clone.setFrameGroups(src->hasFrameGroups());
  clone.markDirty();

  registry_->addClient(clone);
  registry_->backupVersion(new_version);
  populateVersionData();
  selectClient(new_version);
}

void ClientConfigurationDialog::deleteClient(uint32_t version) {
  if (version == 0)
    return;
  pending_deleted_.insert(version);
  editor_->clearPropertyStates(version);
  populateVersionData();
  if (active_version_ == version) {
    active_version_ = 0;
    if (!filtered_versions_.empty())
      selectClient(filtered_versions_[0]);
  }
}

bool ClientConfigurationDialog::validateBeforeSave() {
  std::unordered_set<std::string> names;
  for (const auto &[num, cv] : registry_->getVersionsMap()) {
    if (pending_deleted_.count(num))
      continue;
    if (cv.getName().empty()) {
      validation_error_ = std::format("Client version {} has an empty name.", num);
      return false;
    }
    if (!names.insert(cv.getName()).second) {
      validation_error_ =
          std::format("Duplicate client name: \"{}\".", cv.getName());
      return false;
    }
  }
  return true;
}

bool ClientConfigurationDialog::saveAll() {
  if (!registry_)
    return false;
  if (!validateBeforeSave()) {
    ImGui::OpenPopup("Validation Error");
    return false;
  }
  for (auto id : pending_deleted_)
    registry_->removeClient(id);

  Services::ClientVersionsData data;
  data.versions = registry_->getVersionsMap();
  data.otb_to_version = registry_->getOtbMapping();
  data.default_version = registry_->getDefaultVersion();

  if (!Services::ClientVersionPersistence::saveToJson(registry_->getJsonPath(), data)) {
    spdlog::error("Failed to save clients.json");
    return false;
  }

  for (const auto &[num, _] : registry_->getVersionsMap()) {
    if (auto *cv = registry_->getVersion(num))
      cv->clearDirty();
  }
  for (const auto &[num, _] : registry_->getVersionsMap())
    editor_->markAllPendingAsSaved(num);

  pending_deleted_.clear();
  if (config_) {
    registry_->savePathsToConfig(*config_);
    config_->save();
  }
  spdlog::info("Saved {} client versions", registry_->getVersionsMap().size());
  return true;
}

void ClientConfigurationDialog::discardChanges() {
  for (const auto &[num, _] : registry_->getVersionsMap()) {
    if (auto *cv = registry_->getVersion(num)) {
      if (cv->isDirty()) {
        cv->restore();
        cv->clearDirty();
      }
    }
  }
  pending_deleted_.clear();
  search_filter_.clear();
  search_buf_[0] = '\0';
  populateVersionData();
  if (active_version_ != 0) {
    auto *cv = registry_->getVersion(active_version_);
    if (cv) {
      editor_->syncFromClient(*cv);
      editor_->setActiveVersion(active_version_);
    }
  } else if (!filtered_versions_.empty()) {
    selectClient(filtered_versions_[0]);
  }
}

// Render

bool ClientConfigurationDialog::render() {
  if (!is_open_ || !registry_)
    return false;

  ImGuiIO &io = ImGui::GetIO();
  ImVec2 win_size(1180, 780);
  ImVec2 win_pos((io.DisplaySize.x - win_size.x) * 0.5f,
                 (io.DisplaySize.y - win_size.y) * 0.5f);
  ImGui::SetNextWindowPos(win_pos, ImGuiCond_Appearing);
  ImGui::SetNextWindowSize(win_size, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(900, 500), ImVec2(FLT_MAX, FLT_MAX));

  pushCompactStyle();

  bool open = true;
  ImGui::Begin("Client Configuration", &open,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

  if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    is_open_ = false;
  if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
    if (saveAll())
      populateVersionData();
  }

  renderTitleBar();
  renderToolbar();
  ImGui::Separator();
  renderBody();

  if (ImGui::BeginPopupModal("Validation Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please fix the following before saving:");
    ImGui::TextWrapped("%s", validation_error_.c_str());
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(100, 0)))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::End();
  popCompactStyle();

  if (!open)
    is_open_ = false;
  return is_open_;
}

void ClientConfigurationDialog::renderTitleBar() {
  float title_height = 32.0f;
  ImGui::BeginChild("##titlebar", ImVec2(0, title_height), ImGuiChildFlags_None);

  ImGui::SetCursorPosY(6);
  ImGui::SetCursorPosX(10);
  ImGui::TextColored(kTextOffWhite, "Client Configuration");

  float close_x = ImGui::GetWindowWidth() - 30;
  ImGui::SetCursorPos(ImVec2(close_x, 4));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 0.6f));
  if (ImGui::Button(ICON_FA_XMARK "##close", ImVec2(24, 24)))
    is_open_ = false;
  ImGui::PopStyleColor(2);

  ImGui::EndChild();
}

void ClientConfigurationDialog::renderToolbar() {
  float toolbar_h = 48.0f;
  ImGui::BeginChild("##toolbar", ImVec2(0, toolbar_h), ImGuiChildFlags_None);

  ImGui::SetCursorPosY(6);

  ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBlueActive);
  if (ImGui::Button(ICON_FA_PLUS "  Add", ImVec2(100, 32)))
    addClient();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_COPY "  Duplicate", ImVec2(120, 32)))
    duplicateClient();

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, kRedDelete);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kRedHover);
  if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(100, 32)))
    deleteClient(active_version_);
  ImGui::PopStyleColor(2);

  ImGui::EndChild();
}

void ClientConfigurationDialog::renderBody() {
  float body_h = ImGui::GetContentRegionAvail().y - 48.0f;
  ImGui::BeginChild("##body", ImVec2(0, body_h), ImGuiChildFlags_None);

  renderLeftSidebar();
  ImGui::SameLine(0, 8);
  renderRightPanel();

  ImGui::EndChild();
  renderFooterStatus();
}

void ClientConfigurationDialog::renderLeftSidebar() {
  ImGui::BeginChild("##leftbar", ImVec2(350, 0), ImGuiChildFlags_Borders);

  if (!version_groups_.empty()) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    int cols = 5;
    for (size_t i = 0; i < version_groups_.size(); ++i) {
      if (i > 0 && i % cols != 0)
        ImGui::SameLine();
      auto &grp = version_groups_[i];
      if (grp.visible) {
        ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
      }
      if (ImGui::Button(grp.label.c_str(), ImVec2(50, 0))) {
        grp.visible = !grp.visible;
        populateVersionData();
        if (active_version_ != 0) {
          bool still_visible = false;
          for (const auto &g : version_groups_) {
            if (!g.visible) continue;
            for (auto v : g.versions) {
              if (v == active_version_) { still_visible = true; break; }
            }
            if (still_visible) break;
          }
          if (!still_visible && !filtered_versions_.empty())
            selectClient(filtered_versions_[0]);
        }
      }
      if (grp.visible)
        ImGui::PopStyleColor(2);
    }
    ImGui::PopStyleVar();
  }

  ImGui::Separator();

  ImGui::Columns(4, "##ver_cols4", false);
  ImGui::SetColumnWidth(0, 170);
  ImGui::SetColumnWidth(1, 55);
  ImGui::SetColumnWidth(2, 55);
  ImGui::TextColored(kTextMuted, "Name");
  ImGui::NextColumn();
  ImGui::TextColored(kTextMuted, "Ver");
  ImGui::NextColumn();
  ImGui::TextColored(kTextMuted, "Type");
  ImGui::NextColumn();
  ImGui::TextColored(kTextMuted, "Cfg");
  ImGui::NextColumn();
  ImGui::Columns(1);
  ImGui::Separator();

  renderVersionList();

  ImGui::Separator();
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##search_bottom", ICON_FA_MAGNIFYING_GLASS " Search...",
                               search_buf_, sizeof(search_buf_))) {
    search_filter_ = toLower(search_buf_);
    populateVersionData();
  }

  ImGui::EndChild();
}

void ClientConfigurationDialog::renderVersionList() {
  std::vector<uint32_t> all_versions;
  for (const auto &grp : version_groups_) {
    if (!grp.visible) continue;
    for (auto v : grp.versions)
      all_versions.push_back(v);
  }
  std::sort(all_versions.begin(), all_versions.end());

  ImGui::BeginChild("##verlist", ImVec2(0, -34), ImGuiChildFlags_None);
  if (all_versions.empty()) {
    ImGui::TextDisabled("  No matching clients");
  }
  for (uint32_t ver_num : all_versions) {
    auto *cv = registry_->getVersion(ver_num);
    if (!cv)
      continue;

    bool sel = (active_version_ == ver_num);
    bool dirty = cv->isDirty();
    if (sel) {
      ImGui::PushStyleColor(ImGuiCol_Header, kBlueAccent);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kBlueHover);
    } else if (dirty) {
      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.50f, 0.42f, 0.14f, 1.0f));
    }

    ImGui::PushID(static_cast<int>(ver_num));

    ImGui::Columns(4, "##verlist_cols4", false);
    ImGui::SetColumnWidth(0, 170);
    ImGui::SetColumnWidth(1, 55);
    ImGui::SetColumnWidth(2, 55);

    std::string name_display = cv->getName();
    if (cv->isDirty())
      name_display += " *";
    if (ImGui::Selectable(name_display.c_str(), sel,
                          ImGuiSelectableFlags_AllowDoubleClick |
                              ImGuiSelectableFlags_SpanAllColumns)) {
      selectClient(ver_num);
    }

    if (ImGui::BeginPopupContextItem(std::format("ctx_{}", ver_num).c_str())) {
      if (ImGui::MenuItem("Duplicate"))
        duplicateClient();
      if (ImGui::MenuItem("Delete"))
        deleteClient(ver_num);
      ImGui::EndPopup();
    }

    ImGui::NextColumn();
    ImGui::TextColored(kTextMuted, "%u", cv->getVersion());

    ImGui::NextColumn();
    const char* type_str = "???";
    switch (cv->getDataSource()) {
      case Domain::ItemDataSource::OTB: type_str = "OTB"; break;
      case Domain::ItemDataSource::SRV: type_str = "SRV"; break;
      case Domain::ItemDataSource::DAT: type_str = "DAT"; break;
    }
    ImGui::TextColored(kTextMuted, "%s", type_str);

    ImGui::NextColumn();
    bool configured = !cv->getClientPath().empty();
    if (configured) {
      ImGui::TextColored(kGreenStatus, ICON_FA_CHECK);
    }

    ImGui::Columns(1);

    ImGui::PopID();

    if (sel)
      ImGui::PopStyleColor(2);
    else if (dirty)
      ImGui::PopStyleColor();
  }
  ImGui::EndChild();
}

void ClientConfigurationDialog::renderRightPanel() {
  ImGui::BeginChild("##rightpanel", ImVec2(0, 0), ImGuiChildFlags_Borders);

  if (active_version_ == 0) {
    ImGui::SetCursorPosY(40);
    ImGui::TextColored(kTextMuted,
                       "   Select a client version from the list on the left "
                       "to edit its identity, files, compatibility, and "
                       "feature flags.");
  } else {
    editor_->render();
    editor_->renderStatusBar();
  }

  ImGui::EndChild();
}

void ClientConfigurationDialog::renderFooterStatus() {
  float footer_h = 42.0f;
  ImGui::BeginChild("##footer", ImVec2(0, footer_h), ImGuiChildFlags_None);
  ImGui::SetCursorPosY(8);

  float btn_x = ImGui::GetWindowWidth() - 310;
  ImGui::SameLine(btn_x);

  if (ImGui::Button(ICON_FA_FLOPPY_DISK " Apply", ImVec2(90, 28))) {
    if (saveAll())
      populateVersionData();
  }

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
  if (ImGui::Button(ICON_FA_CHECK " Save", ImVec2(90, 28))) {
    if (saveAll()) {
      populateVersionData();
      is_open_ = false;
    }
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_XMARK " Close", ImVec2(90, 28))) {
    bool has_dirty = false;
    if (registry_) {
      for (const auto &[num, _] : registry_->getVersionsMap()) {
        if (auto *cv = registry_->getVersion(num)) {
          if (cv->isDirty()) { has_dirty = true; break; }
        }
      }
    }
    if (has_dirty) {
      ImGui::OpenPopup("Unsaved Changes");
    } else {
      is_open_ = false;
    }
  }

  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION " You have unsaved changes.");
    ImGui::TextUnformatted("Discard them and close?");
    ImGui::Spacing();
    if (ImGui::Button("Discard && Close", ImVec2(140, 0))) {
      discardChanges();
      is_open_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
