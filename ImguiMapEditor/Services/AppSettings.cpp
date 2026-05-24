#include "AppSettings.h"
#include "ConfigService.h"
#include "UI/Core/Theme.h" // needed for ThemeType enum values in constructor
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

AppSettings::AppSettings() : theme(ThemeType::ModernDark) {}

void AppSettings::loadFromConfig(const ConfigService &config) {
  theme = static_cast<ThemeType>(
      config.get<int>("app.theme", static_cast<int>(ThemeType::ModernDark)));
  paletteIconSize = config.get<float>("app.paletteIconSize", 48.0f);
  openPaletteNames = config.get<std::string>("app.openPaletteNames", "");
  spdlog::info(
      "AppSettings: Loaded theme={}, paletteIconSize={}, openPalettes={}",
      static_cast<int>(theme), paletteIconSize, openPaletteNames);
}

void AppSettings::saveToConfig(ConfigService &config) const {
  spdlog::info(
      "AppSettings: Saving theme={}, paletteIconSize={}, openPalettes={}",
      static_cast<int>(theme), paletteIconSize, openPaletteNames);
  config.set("app.theme", static_cast<int>(theme));
  config.set("app.paletteIconSize", paletteIconSize);
  config.set("app.openPaletteNames", openPaletteNames);
}

// apply() removed — theme application belongs in the UI/Presentation layer.
// Callers should use UI::ApplyTheme(settings.theme) directly.

} // namespace Services
} // namespace MapEditor
