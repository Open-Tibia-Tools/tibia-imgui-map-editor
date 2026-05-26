#pragma once
#include <memory>

#include "Services/AppSettings.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include "Services/HotkeyRegistry.h"
#include "Services/OtbmSettings.h"
#include "Services/RecentLocationsService.h"
#include "Services/ViewSettings.h"
#include "Domain/SelectionSettings.h"

namespace MapEditor::Services {

class SettingsRegistry {
public:
  SettingsRegistry();
  ~SettingsRegistry();

  bool load();
  void save();

  // Accessors
  ConfigService &getConfig();
  const ConfigService &getConfig() const;

  ClientVersionRegistry &getVersionRegistry();
  const ClientVersionRegistry &getVersionRegistry() const;

  RecentLocationsService &getRecentLocations();
  const RecentLocationsService &getRecentLocations() const;

  ViewSettings &getViewSettings();
  const ViewSettings &getViewSettings() const;

  AppSettings &getAppSettings();
  const AppSettings &getAppSettings() const;

  OtbmSettings &getOtbmSettings();
  const OtbmSettings &getOtbmSettings() const;

  Domain::SelectionSettings &getSelectionSettings();
  const Domain::SelectionSettings &getSelectionSettings() const;

  HotkeyRegistry &getHotkeyRegistry();
  const HotkeyRegistry &getHotkeyRegistry() const;

private:
  std::unique_ptr<ConfigService> config_service_;
  std::unique_ptr<ClientVersionRegistry> version_registry_;
  std::unique_ptr<RecentLocationsService> recent_locations_;

  ViewSettings view_settings_;
  AppSettings app_settings_;
  OtbmSettings otbm_settings_;
  Domain::SelectionSettings selection_settings_;
  HotkeyRegistry hotkey_registry_;
};

} // namespace MapEditor::Services
