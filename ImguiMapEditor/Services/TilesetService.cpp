#include "TilesetService.h"

#include "../Brushes/BrushRegistry.h"
#include "../IO/MaterialsXmlReader.h"
#include "../IO/PaletteXmlReader.h"
#include "../IO/TilesetXmlReader.h"
#include "Brushes/BorderLookupService.h"
#include "Brushes/CarpetLookupService.h"
#include "Brushes/TableLookupService.h"
#include "Brushes/WallLookupService.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Services {

TilesetService::TilesetService(MapEditor::Brushes::BrushRegistry &brushRegistry)
    : brushRegistry_(brushRegistry),
      borderLookup_(std::make_unique<MapEditor::Services::Brushes::BorderLookupService>()),
      wallLookup_(std::make_unique<MapEditor::Services::Brushes::WallLookupService>()),
      tableLookup_(std::make_unique<MapEditor::Services::Brushes::TableLookupService>()),
      carpetLookup_(std::make_unique<MapEditor::Services::Brushes::CarpetLookupService>()) {}

TilesetService::~TilesetService() = default;

bool TilesetService::loadTilesets(const std::filesystem::path &dataPath) {
  // Look for tileset XML files in the data directory
  std::filesystem::path tilesetsPath = dataPath / "tilesets";

  if (!std::filesystem::exists(tilesetsPath)) {
    spdlog::warn("[TilesetService] Tilesets directory not found: {}",
                 tilesetsPath.string());
    return false;
  }

  // Pass owned tilesetRegistry_ to reader
  IO::TilesetXmlReader reader(brushRegistry_, tilesetRegistry_);
  int loadedCount = 0;

  // Use recursive iterator to find tilesets in subdirectories
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(tilesetsPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".xml") {
      if (reader.loadTilesetFile(entry.path())) {
        loadedCount++;
      }
    }
  }

  spdlog::info("[TilesetService] Loaded {} tileset files", loadedCount);
  spdlog::info("[TilesetService] Total tilesets in registry: {}",
               tilesetRegistry_.getAllTilesets().size());

  loaded_ = loadedCount > 0;
  return loaded_;
}

bool TilesetService::loadPalettes(const std::filesystem::path &dataPath) {
  std::filesystem::path palettesPath = dataPath / "palettes.xml";

  if (!std::filesystem::exists(palettesPath)) {
    spdlog::warn("[TilesetService] palettes.xml not found at: {}",
                 palettesPath.string());
    return false;
  }

  // Pass owned registries to reader
  IO::PaletteXmlReader reader(tilesetRegistry_, paletteRegistry_);
  if (!reader.load(palettesPath)) {
    spdlog::error("[TilesetService] Failed to load palettes.xml");
    return false;
  }

  spdlog::info("[TilesetService] Loaded {} palettes",
               paletteRegistry_.getPaletteNames().size());

  return true;
}

bool TilesetService::loadMaterials(const std::filesystem::path &dataPath) {
  const auto materialsPath = dataPath / "materials.xml";
  if (!std::filesystem::exists(materialsPath)) {
    spdlog::warn("[TilesetService] materials.xml not found at: {}",
                 materialsPath.string());
    return false;
  }

  tilesetRegistry_.clear();
  paletteRegistry_.clear();
  brushRegistry_.clear();
  brushRegistry_.setClientDataService(clientData_);

  IO::MaterialsXmlReader reader(brushRegistry_, tilesetRegistry_, paletteRegistry_,
                                borderLookup_.get(), wallLookup_.get(),
                                tableLookup_.get(), carpetLookup_.get(),
                                clientData_);
  loaded_ = reader.load(materialsPath);
  return loaded_;
}

} // namespace MapEditor::Services
