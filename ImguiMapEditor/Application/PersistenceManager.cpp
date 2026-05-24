#include "PersistenceManager.hpp"
#include "Application/ClientVersionManager.h"
#include "Application/PlatformManager.hpp"
#include "Services/SettingsRegistry.h"

namespace MapEditor {

void PersistenceManager::saveApplicationState(Services::SettingsRegistry& settings,
                                              const PlatformManager& platform,
                                              const ClientVersionManager& version_manager) {
    // Save window state (size, position, maximized)
    platform.saveWindowState(settings.getConfig());

    // Save secondary client settings (must be done before registry save)
    if (version_manager.hasSecondaryClient()) {
        version_manager.getSecondaryClient()->saveSettingsToConfig(settings.getConfig());
    }

    // Save all other settings via registry (Recent files, View settings, etc.)
    settings.save();
}

} // namespace MapEditor
