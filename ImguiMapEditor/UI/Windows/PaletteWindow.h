#pragma once

#include "../../Domain/Palette/Palette.h"
#include "../../Domain/Tileset/TilesetRegistry.h"
#include "../Widgets/TilesetGridWidget.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace MapEditor {

namespace Services {
class ClientDataService;
class SpriteManager;
struct AppSettings;
} // namespace Services

namespace Brushes {
class BrushController;
} // namespace Brushes

namespace UI {

/**
 * Dockable window showing tilesets for a specific palette.
 *
 * The palette name is shown as the window title.
 * Contains a dropdown to select tileset and a TilesetGridWidget
 * to display the entries for the selected tileset.
 *
 * Example: "Boss Encounters" palette with "Bosses" and "Magic" tilesets.
 */
class PaletteWindow {
public:
  explicit PaletteWindow(const std::string &paletteName);
  ~PaletteWindow();

  /**
   * Initialize with required services.
   */
  void initialize(Services::ClientDataService *clientData,
                  Services::SpriteManager *spriteManager,
                  Brushes::BrushController *brushController,
                  Domain::Tileset::TilesetRegistry &tilesetRegistry,
                  Domain::Palette::PaletteRegistry &paletteRegistry,
                  Services::AppSettings *appSettings = nullptr);

  /**
   * Render the window. Returns false if window was closed.
   */
  bool render();

  // Visibility
  bool isVisible() const { return visible_; }
  void setVisible(bool v) { visible_ = v; }
  void toggleVisible() { visible_ = !visible_; }

  const std::string &getPaletteName() const { return paletteName_; }

private:
  void refreshTilesetList();
  void selectTileset(size_t index);
  void handleJumpToTileset(const std::string &tilesetName,
                           const std::string &brushName);
  void syncActiveBrushSelection();
  [[nodiscard]] Domain::Tileset::Tileset *getTilesetAt(size_t index) const;
  [[nodiscard]] std::string getTilesetDisplayLabel(size_t index) const;
  std::optional<size_t> findTilesetIndexForBrush(
      const Brushes::IBrush *brush) const;

  std::string paletteName_;
  Domain::Palette::Palette *palette_ = nullptr;
  bool visible_ = false;
  bool initialized_ = false;

  // Tileset list for this palette
  std::vector<std::string> tilesetNames_;
  int selectedTilesetIndex_ = 0;

  // The grid widget for entry display
  TilesetGridWidget gridWidget_;

  // Services (non-owning)
  Services::ClientDataService *clientData_ = nullptr;
  Services::SpriteManager *spriteManager_ = nullptr;
  Brushes::BrushController *brushController_ = nullptr;
  Services::AppSettings *appSettings_ = nullptr;
  Domain::Tileset::TilesetRegistry *tilesetRegistry_ = nullptr;
  Domain::Palette::PaletteRegistry *paletteRegistry_ = nullptr;
  const Brushes::IBrush *syncedActiveBrush_ = nullptr;
};

} // namespace UI
} // namespace MapEditor
