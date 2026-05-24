#pragma once

namespace MapEditor::Services {
class SettingsRegistry;
}

namespace MapEditor {

class PlatformManager;
class ClientVersionManager;

/**
 * Manages the persistence of application state during shutdown.
 * Coordinates saving of window state, secondary client settings, and general registry settings.
 */
class PersistenceManager {
public:
  PersistenceManager() = default;

  /**
   * Save all application state and settings.
   *
   * @param settings The settings registry containing configuration.
   * @param platform The platform manager handling window state.
   * @param version_manager The client version manager handling secondary clients.
   */
  static void saveApplicationState(Services::SettingsRegistry& settings,
                            const PlatformManager& platform,
                            const ClientVersionManager& version_manager) ;
};

} // namespace MapEditor
