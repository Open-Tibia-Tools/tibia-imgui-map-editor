#include "Services/SettingsRegistry.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Services {

SettingsRegistry::SettingsRegistry() = default;
SettingsRegistry::~SettingsRegistry() = default;

bool SettingsRegistry::load() {
  config_service_ = std::make_unique<ConfigService>();
  config_service_->load();

  version_registry_ = std::make_unique<ClientVersionRegistry>();
  // We can attempt to load defaults, but failure here might be handled gracefully depending on app requirements.
  // Original Application::loadClientVersions returned false on failure, so we'll propagate that.
  if (!version_registry_->loadDefaults(*config_service_)) {
    return false;
  }

  recent_locations_ = std::make_unique<RecentLocationsService>();
  recent_locations_->loadFromConfig(*config_service_);

  view_settings_.loadFromConfig(*config_service_);
  // SelectionSettings serialization (was in Domain/SelectionSettings.cpp — moved here to break Domain→Services dependency)
  selection_settings_.floor_scope = static_cast<Domain::SelectionFloorScope>(
      config_service_->get<int>("selection.floor_scope", 0));
  selection_settings_.use_pixel_perfect = config_service_->get<bool>("selection.use_pixel_perfect", false);

  // AppSettings loading doesn't require ImGui context, only applying it does.
  app_settings_.loadFromConfig(*config_service_);

  hotkey_registry_ = HotkeyRegistry::loadOrCreateDefaults();

  spdlog::info("Configuration and settings loaded");
  return true;
}

void SettingsRegistry::save() {
  if (!config_service_) return;

  if (version_registry_)
    version_registry_->savePathsToConfig(*config_service_);
  if (recent_locations_)
    recent_locations_->saveToConfig(*config_service_);

  view_settings_.saveToConfig(*config_service_);
  // SelectionSettings serialization (moved from Domain to break Domain→Services dependency)
  config_service_->set("selection.floor_scope", static_cast<int>(selection_settings_.floor_scope));
  config_service_->set("selection.use_pixel_perfect", selection_settings_.use_pixel_perfect);
  app_settings_.saveToConfig(*config_service_);

  config_service_->save();
}

ConfigService &SettingsRegistry::getConfig() { return *config_service_; }
const ConfigService &SettingsRegistry::getConfig() const {
  return *config_service_;
}

ClientVersionRegistry &SettingsRegistry::getVersionRegistry() {
  return *version_registry_;
}
const ClientVersionRegistry &SettingsRegistry::getVersionRegistry() const {
  return *version_registry_;
}

RecentLocationsService &SettingsRegistry::getRecentLocations() {
  return *recent_locations_;
}
const RecentLocationsService &SettingsRegistry::getRecentLocations() const {
  return *recent_locations_;
}

ViewSettings &SettingsRegistry::getViewSettings() { return view_settings_; }
const ViewSettings &SettingsRegistry::getViewSettings() const {
  return view_settings_;
}

AppSettings &SettingsRegistry::getAppSettings() { return app_settings_; }
const AppSettings &SettingsRegistry::getAppSettings() const {
  return app_settings_;
}

Domain::SelectionSettings &SettingsRegistry::getSelectionSettings() {
  return selection_settings_;
}
const Domain::SelectionSettings &SettingsRegistry::getSelectionSettings() const {
  return selection_settings_;
}

HotkeyRegistry &SettingsRegistry::getHotkeyRegistry() {
  return hotkey_registry_;
}
const HotkeyRegistry &SettingsRegistry::getHotkeyRegistry() const {
  return hotkey_registry_;
}

} // namespace MapEditor::Services
