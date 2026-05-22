#pragma once

#include "../Domain/Palette/Palette.h"
#include "../Domain/Tileset/TilesetRegistry.h"
#include <filesystem>
#include <memory>

namespace MapEditor::Brushes {
class BrushRegistry;
}

namespace MapEditor::Services {

class ClientDataService;

namespace Brushes {
class BorderLookupService;
class WallLookupService;
class TableLookupService;
class CarpetLookupService;
} // namespace Brushes

/**
 * Service responsible for loading and managing tilesets and palettes.
 *
 * This service:
 * - Owns the TilesetRegistry and PaletteRegistry
 * - Loads individual tileset XML files from a directory
 * - Loads palettes from palettes.xml
 * - Provides access to registries via dependency injection
 */
class TilesetService {
public:
  explicit TilesetService(MapEditor::Brushes::BrushRegistry &brushRegistry);
  ~TilesetService();

  // Non-copyable
  TilesetService(const TilesetService &) = delete;
  TilesetService &operator=(const TilesetService &) = delete;

  /**
   * Load tilesets from a data directory.
   * Looks for tileset XML files in dataPath/tilesets/
   *
   * @param dataPath Root data path (contains tilesets/ folder)
   * @return true if at least one tileset was loaded
   */
  bool loadTilesets(const std::filesystem::path &dataPath);

  /**
   * Load palettes from palettes.xml.
   * Must be called AFTER loadTilesets() since palettes reference tilesets.
   *
   * @param dataPath Root data path (contains palettes.xml)
   * @return true if palettes were loaded successfully
   */
  bool loadPalettes(const std::filesystem::path &dataPath);

  /**
   * Load the full materials graph rooted at materials.xml.
   * This loads border templates, brushes, tilesets, and palettes in reference
   * order.
   */
  bool loadMaterials(const std::filesystem::path &dataPath);

  void setClientDataService(ClientDataService *clientData) {
    clientData_ = clientData;
  }

  /**
   * Check if tilesets have been loaded.
   */
  bool isLoaded() const { return loaded_; }

  /**
   * Get the tileset registry (for dependency injection).
   */
  Domain::Tileset::TilesetRegistry &getTilesetRegistry() {
    return tilesetRegistry_;
  }
  const Domain::Tileset::TilesetRegistry &getTilesetRegistry() const {
    return tilesetRegistry_;
  }

  /**
   * Get the palette registry (for dependency injection).
   */
  Domain::Palette::PaletteRegistry &getPaletteRegistry() {
    return paletteRegistry_;
  }
  const Domain::Palette::PaletteRegistry &getPaletteRegistry() const {
    return paletteRegistry_;
  }

private:
  MapEditor::Brushes::BrushRegistry &brushRegistry_;
  Domain::Tileset::TilesetRegistry tilesetRegistry_;
  Domain::Palette::PaletteRegistry paletteRegistry_;
  ClientDataService *clientData_ = nullptr;
  std::unique_ptr<MapEditor::Services::Brushes::BorderLookupService>
      borderLookup_;
  std::unique_ptr<MapEditor::Services::Brushes::WallLookupService> wallLookup_;
  std::unique_ptr<MapEditor::Services::Brushes::TableLookupService>
      tableLookup_;
  std::unique_ptr<MapEditor::Services::Brushes::CarpetLookupService>
      carpetLookup_;
  bool loaded_ = false;
};

} // namespace MapEditor::Services
