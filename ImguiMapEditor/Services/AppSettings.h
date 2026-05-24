#pragma once
#include <cstdint>
#include <string>

// Forward declare ThemeType from Theme.h to avoid including imgui
enum class ThemeType;

namespace MapEditor {
namespace Services {

// Forward declaration
class ConfigService;

/**
 * Application-wide settings that persist between sessions.
 * Holds UI preferences, theme, window state, etc.
 */
struct AppSettings {
  // Theme (ThemeType is in global namespace from theme.h)
  ThemeType theme;

  // Palette icon size (32-128 pixels)
  float paletteIconSize = 48.0f;

  // Open palette names (comma-separated list of visible palette window names)
  std::string openPaletteNames;

  // Constructor with default theme
  AppSettings();

  // Persistence
  void loadFromConfig(const ConfigService &config);
  void saveToConfig(ConfigService &config) const;

  // Theme application moved to UI layer — call UI::ApplyTheme(settings.theme)
};

} // namespace Services
} // namespace MapEditor
