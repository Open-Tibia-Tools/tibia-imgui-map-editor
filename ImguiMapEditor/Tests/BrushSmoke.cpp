#include "Brushes/BrushController.h"
#include "Brushes/BrushRegistry.h"
#include "Brushes/Core/IBrush.h"
#include "Brushes/Types/GroundBrush.h"
#include "Brushes/Types/RawBrush.h"
#include "Application/EditorSession.h"
#include "Controllers/MapInputController.h"
#include "Domain/ChunkedMap.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/ItemType.h"
#include "Domain/MapInstance.h"
#include "Domain/SelectionSettings.h"
#include "Domain/Tileset/TilesetEntry.h"
#include "IO/HouseXmlReader.h"
#include "IO/HouseXmlWriter.h"
#include "IO/Otbm/OtbmReader.h"
#include "IO/Otbm/OtbmWriter.h"
#include "IO/SpawnXmlReader.h"
#include "IO/SpawnXmlWriter.h"
#include "IO/TilesetXmlReader.h"
#include "Services/Brushes/BorderLookupService.h"
#include "Services/Brushes/CarpetLookupService.h"
#include "Services/Brushes/TableLookupService.h"
#include "Services/Brushes/WallLookupService.h"
#include "Services/BrushSettingsService.h"
#include "Services/HotkeyRegistry.h"
#include "Services/Preview/BrushPreviewFactory.h"
#include "Services/Preview/IPreviewProvider.h"
#include "Services/Preview/PreviewService.h"
#include "Services/Selection/SelectionService.h"
#include "Services/TilesetService.h"
#include "UI/Utils/BrushPreviewResolver.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

using MapBrushType = MapEditor::Brushes::BrushType;
using PickMode = MapEditor::Brushes::BrushPickMode;
using EdgeType = MapEditor::Brushes::EdgeType;
using TableAlign = MapEditor::Brushes::TableAlign;
using TileNeighbor = MapEditor::Brushes::TileNeighbor;
using WallAlign = MapEditor::Brushes::WallAlign;
using WallNeighbor = MapEditor::Brushes::WallNeighbor;

static_assert(static_cast<uint8_t>(EdgeType::N) == 1);
static_assert(static_cast<uint8_t>(EdgeType::E) == 2);
static_assert(static_cast<uint8_t>(EdgeType::S) == 3);
static_assert(static_cast<uint8_t>(EdgeType::W) == 4);
static_assert(static_cast<uint8_t>(EdgeType::CNW) == 5);
static_assert(static_cast<uint8_t>(EdgeType::CNE) == 6);
static_assert(static_cast<uint8_t>(EdgeType::CSW) == 7);
static_assert(static_cast<uint8_t>(EdgeType::CSE) == 8);
static_assert(static_cast<uint8_t>(EdgeType::DNW) == 9);
static_assert(static_cast<uint8_t>(EdgeType::DNE) == 10);
static_assert(static_cast<uint8_t>(EdgeType::DSE) == 11);
static_assert(static_cast<uint8_t>(EdgeType::DSW) == 12);
static_assert(static_cast<uint8_t>(TableAlign::North) == 0);
static_assert(static_cast<uint8_t>(TableAlign::South) == 1);
static_assert(static_cast<uint8_t>(TableAlign::East) == 2);
static_assert(static_cast<uint8_t>(TableAlign::West) == 3);
static_assert(static_cast<uint8_t>(TableAlign::Horizontal) == 4);
static_assert(static_cast<uint8_t>(TableAlign::Vertical) == 5);
static_assert(static_cast<uint8_t>(TableAlign::Alone) == 6);
static_assert(static_cast<uint8_t>(WallAlign::Pole) == 0);
static_assert(static_cast<uint8_t>(WallAlign::SouthEnd) == 1);
static_assert(static_cast<uint8_t>(WallAlign::EastEnd) == 2);
static_assert(static_cast<uint8_t>(WallAlign::NorthwestDiagonal) == 3);
static_assert(static_cast<uint8_t>(WallAlign::WestEnd) == 4);
static_assert(static_cast<uint8_t>(WallAlign::NortheastDiagonal) == 5);
static_assert(static_cast<uint8_t>(WallAlign::Horizontal) == 6);
static_assert(static_cast<uint8_t>(WallAlign::SouthT) == 7);
static_assert(static_cast<uint8_t>(WallAlign::NorthEnd) == 8);
static_assert(static_cast<uint8_t>(WallAlign::Vertical) == 9);
static_assert(static_cast<uint8_t>(WallAlign::SouthwestDiagonal) == 10);
static_assert(static_cast<uint8_t>(WallAlign::EastT) == 11);
static_assert(static_cast<uint8_t>(WallAlign::SoutheastDiagonal) == 12);
static_assert(static_cast<uint8_t>(WallAlign::WestT) == 13);
static_assert(static_cast<uint8_t>(WallAlign::NorthT) == 14);
static_assert(static_cast<uint8_t>(WallAlign::Intersection) == 15);

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

MapEditor::Brushes::IBrush *
findBrushByType(MapEditor::Brushes::BrushRegistry &registry,
                MapBrushType type) {
  for (auto *brush : registry.getAllBrushes()) {
    if (brush && brush->getType() == type) {
      return brush;
    }
  }
  return nullptr;
}

MapEditor::Brushes::IBrush *
findBrush(MapEditor::Brushes::BrushRegistry &registry, std::string_view preferred,
          MapBrushType fallbackType) {
  if (auto *brush = registry.getBrush(std::string(preferred))) {
    return brush;
  }
  return findBrushByType(registry, fallbackType);
}

bool tileContainsItemId(const MapEditor::Domain::Tile &tile, uint16_t itemId) {
  if (tile.hasGround() && tile.getGround()->getServerId() == itemId) {
    return true;
  }

  for (const auto &item : tile.getItems()) {
    if (item && item->getServerId() == itemId) {
      return true;
    }
  }

  return false;
}

size_t countOwnedItems(const MapEditor::Domain::Tile &tile,
                       const MapEditor::Brushes::IBrush &brush) {
  size_t count = 0;
  for (const auto &item : tile.getItems()) {
    if (item && brush.ownsItem(item.get())) {
      ++count;
    }
  }
  return count;
}

size_t countTiles(const MapEditor::Domain::ChunkedMap &map) {
  size_t count = 0;
  map.forEachTile([&count](const MapEditor::Domain::Tile *) { ++count; });
  return count;
}

std::optional<MapEditor::Domain::Position>
findTileWithItemsNear(const MapEditor::Domain::ChunkedMap &map,
                      const MapEditor::Domain::Position &center, int radius) {
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      const MapEditor::Domain::Position pos{center.x + dx, center.y + dy,
                                            center.z};
      const auto *tile = map.getTile(pos);
      if (tile && (tile->hasGround() || tile->getItemCount() > 0)) {
        return pos;
      }
    }
  }
  return std::nullopt;
}

void clearTileBrushOwnership(MapEditor::Domain::Tile &tile) {
  if (auto *ground = tile.getGround()) {
    ground->setOwnerBrushId(MapEditor::Brushes::InvalidBrushId);
  }
  for (const auto &item : tile.getItems()) {
    if (item) {
      item->setOwnerBrushId(MapEditor::Brushes::InvalidBrushId);
    }
  }

  tile.setGroundBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setOptionalBorderBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setSpawnBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setCreatureBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setHouseBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setHouseExitBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setWaypointBrushId(MapEditor::Brushes::InvalidBrushId);
  tile.setZoneBrushId(MapEditor::Domain::TileFlag::ProtectionZone,
                      MapEditor::Brushes::InvalidBrushId);
  tile.setZoneBrushId(MapEditor::Domain::TileFlag::NoPvp,
                      MapEditor::Brushes::InvalidBrushId);
  tile.setZoneBrushId(MapEditor::Domain::TileFlag::NoLogout,
                      MapEditor::Brushes::InvalidBrushId);
  tile.setZoneBrushId(MapEditor::Domain::TileFlag::PvpZone,
                      MapEditor::Brushes::InvalidBrushId);
  tile.setZoneBrushId(MapEditor::Domain::TileFlag::Refresh,
                      MapEditor::Brushes::InvalidBrushId);
}

void writeTextFile(const fs::path &path, std::string_view content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("failed to create " + path.string());
  }
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

} // namespace

int main() {
  try {
    const fs::path sourceRoot = BRUSH_SMOKE_SOURCE_DIR;
    const fs::path dataPath = sourceRoot / "sample_data";
    const fs::path tempDir =
        fs::temp_directory_path() / "imgui-mapeditor-brush-smoke";
    std::error_code cleanupError;
    fs::remove_all(tempDir, cleanupError);
    fs::create_directories(tempDir);

    require(fs::exists(dataPath / "materials.xml"),
            "sample_data/materials.xml is missing");

    MapEditor::Brushes::BrushRegistry registry;
    const auto hotkeys = MapEditor::Services::HotkeyRegistry::createDefaults();
    require(hotkeys.findByAction("BRUSH_REFRESH_CURRENT") != nullptr,
            "brush refresh hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_TOGGLE_SELECTION_TOOL") != nullptr,
            "brush selection-toggle hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_RESTORE_LAST") != nullptr,
            "brush previous-brush hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_VARIATION_PREV") != nullptr,
            "brush variation-prev hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_VARIATION_NEXT") != nullptr,
            "brush variation-next hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_SLOT_0") != nullptr,
            "brush slot recall hotkey binding is missing");
    require(hotkeys.findByAction("BRUSH_STORE_SLOT_0") != nullptr,
            "brush slot store hotkey binding is missing");
    require(hotkeys.findByAction("ROTATE_ITEM") != nullptr,
            "rotate-item hotkey binding is missing");
    const auto hotkeyPath = tempDir / "hotkeys.json";
    writeTextFile(hotkeyPath,
                  R"json({"bindings":{"edit":{"SAVE":{"key":"S","mods":["Ctrl"]}}}})json");
    const auto loadedHotkeys = MapEditor::Services::HotkeyRegistry::loadOrCreateDefaults(
        {hotkeyPath.string()});
    require(loadedHotkeys.findByAction("SAVE") != nullptr,
            "custom hotkey binding did not load");
    require(loadedHotkeys.findByAction("BRUSH_TOGGLE_SELECTION_TOOL") != nullptr,
            "custom hotkey load did not merge brush defaults");
    MapEditor::Services::TilesetService tilesetService(registry);
    require(tilesetService.loadMaterials(dataPath),
            "materials.xml failed to load");
    require(!tilesetService.getTilesetRegistry().empty(),
            "no tilesets were registered");
    require(!tilesetService.getPaletteRegistry().empty(),
            "no palettes were registered");

    auto *creatureOthersTileset =
        tilesetService.getTilesetRegistry().getTilesetBySourceFile(
            dataPath / "tilesets/creatures/Others_creatures.xml");
    auto *rawOthersTileset =
        tilesetService.getTilesetRegistry().getTilesetBySourceFile(
            dataPath / "tilesets/raw/Others_raw.xml");
    require(creatureOthersTileset != nullptr,
            "creature Others tileset was not registered by source file");
    require(rawOthersTileset != nullptr,
            "raw Others tileset was not registered by source file");
    require(creatureOthersTileset != rawOthersTileset,
            "same-named creature/raw tilesets were merged together");
    require(creatureOthersTileset->size() < rawOthersTileset->size(),
            "creature Others tileset unexpectedly contains raw-scale entries");

    auto *creaturePalette =
        tilesetService.getPaletteRegistry().getPalette("Creature");
    require(creaturePalette != nullptr, "Creature palette lookup failed");
    require(creaturePalette->getTilesetCount() == 1,
            "Creature palette should resolve to one tileset");
    require(creaturePalette->getTilesetAt(0) == creatureOthersTileset,
            "Creature palette did not bind to the creature Others tileset");
    require(creaturePalette->getTilesetAt(0)->size() < 1000,
            "Creature palette tileset unexpectedly contains raw-scale entries");

    size_t inspectedTilesetBrushes = 0;
    for (const auto &tilesetPtr : tilesetService.getTilesetRegistry().getAllTilesets()) {
      require(tilesetPtr != nullptr, "tileset registry contained a null tileset");
      for (const auto &entry : tilesetPtr->getEntries()) {
        if (!MapEditor::Domain::Tileset::isBrush(entry)) {
          continue;
        }

        const auto *brush = MapEditor::Domain::Tileset::getBrush(entry);
        require(brush != nullptr, "tileset brush entry was null after load");
        [[maybe_unused]] const auto brushType = brush->getType();
        [[maybe_unused]] const auto &brushName = brush->getName();
        [[maybe_unused]] const auto preview = brush->getPreviewDescriptor();
        ++inspectedTilesetBrushes;
      }
    }
    require(inspectedTilesetBrushes > 0,
            "tileset post-load inspection did not visit any brush entries");

    MapEditor::Services::Brushes::BorderLookupService borderLookup;
    MapEditor::Services::Brushes::WallLookupService wallLookup;
    MapEditor::Services::Brushes::TableLookupService tableLookup;
    MapEditor::Services::Brushes::CarpetLookupService carpetLookup;

    const auto northOnly = MapEditor::Services::Brushes::BorderLookupService::unpack(
        borderLookup.getBorderTypes(TileNeighbor::North));
    require(northOnly.size() == 1 && northOnly.front() == EdgeType::N,
            "border lookup north edge contract mismatch");
    const auto eastOnly = MapEditor::Services::Brushes::BorderLookupService::unpack(
        borderLookup.getBorderTypes(TileNeighbor::East));
    require(eastOnly.size() == 1 && eastOnly.front() == EdgeType::E,
            "border lookup east edge contract mismatch");
    const auto northEast = MapEditor::Services::Brushes::BorderLookupService::unpack(
        borderLookup.getBorderTypes(TileNeighbor::North | TileNeighbor::East));
    require(northEast.size() == 1 && northEast.front() == EdgeType::DNE,
            "border lookup north-east diagonal contract mismatch");
    const auto southWest = MapEditor::Services::Brushes::BorderLookupService::unpack(
        borderLookup.getBorderTypes(TileNeighbor::South | TileNeighbor::West));
    require(southWest.size() == 1 && southWest.front() == EdgeType::DSW,
            "border lookup south-west diagonal contract mismatch");
    require(wallLookup.getFullType(WallNeighbor::North) == WallAlign::SouthEnd,
            "wall lookup north contract mismatch");
    require(wallLookup.getFullType(WallNeighbor::West) == WallAlign::EastEnd,
            "wall lookup west contract mismatch");
    require(wallLookup.getFullType(WallNeighbor::North | WallNeighbor::East) ==
                WallAlign::NortheastDiagonal,
            "wall lookup north-east contract mismatch");
    require(tableLookup.getTableType(TileNeighbor::North | TileNeighbor::South) ==
                TableAlign::Vertical,
            "table lookup vertical contract mismatch");
    require(tableLookup.getTableType(TileNeighbor::East | TileNeighbor::West) ==
                TableAlign::Horizontal,
            "table lookup horizontal contract mismatch");
    const auto carpetSouth = MapEditor::Services::Brushes::CarpetLookupService::unpack(
        carpetLookup.getCarpetTypes(TileNeighbor::South));
    require(carpetSouth.size() == 1 && carpetSouth.front() == EdgeType::CSW,
            "carpet lookup south contract mismatch");

    MapEditor::Brushes::BorderBlock groupedBorder;
    groupedBorder.setGroup(77);
    groupedBorder.addItem(EdgeType::N, 60000, 1);
    registry.registerBorderTemplate(9000, std::move(groupedBorder));
    const auto *borderMetadata = registry.getBorderItemMetadata(60000);
    require(borderMetadata != nullptr && borderMetadata->group == 77 &&
                borderMetadata->alignment == EdgeType::N,
            "border item metadata contract mismatch");

    {
      MapEditor::Brushes::BrushRegistry placeholderRegistry;
      MapEditor::Domain::Tileset::TilesetRegistry placeholderTilesets;
      MapEditor::IO::TilesetXmlReader placeholderReader(placeholderRegistry,
                                                        placeholderTilesets);
      const fs::path placeholderTilesetPath = tempDir / "placeholder_tileset.xml";
      const fs::path resolveTriggerPath = tempDir / "resolve_trigger.xml";

      writeTextFile(placeholderTilesetPath,
                    "<tileset name=\"Placeholder Test\"><brush name=\"deferred brush\"/></tileset>");
      require(placeholderReader.loadTilesetFile(placeholderTilesetPath),
              "placeholder tileset load failed");

      auto *placeholderTileset =
          placeholderTilesets.getTileset("Placeholder Test");
      require(placeholderTileset != nullptr && placeholderTileset->size() == 1,
              "placeholder tileset entry missing");
      require(MapEditor::Domain::Tileset::isBrush(
                  placeholderTileset->getEntries().front()),
              "placeholder tileset did not register a brush entry");
      require(MapEditor::Domain::Tileset::getBrush(
                      placeholderTileset->getEntries().front())
                      ->getType() == MapEditor::Brushes::BrushType::Placeholder,
              "placeholder tileset entry was not a placeholder brush");

      auto deferredBrush = std::make_unique<MapEditor::Brushes::GroundBrush>(
          "deferred brush", 0, placeholderRegistry);
      deferredBrush->addGroundItem(101, 1);
      placeholderRegistry.addBrush(std::move(deferredBrush));

      writeTextFile(resolveTriggerPath,
                    "<tileset name=\"Resolve Trigger\"><separator name=\"x\"/></tileset>");
      require(placeholderReader.loadTilesetFile(resolveTriggerPath),
              "placeholder resolve trigger load failed");

      const auto *resolvedBrush = MapEditor::Domain::Tileset::getBrush(
          placeholderTileset->getEntries().front());
      require(resolvedBrush != nullptr &&
                  resolvedBrush->getType() !=
                      MapEditor::Brushes::BrushType::Placeholder &&
                  resolvedBrush ==
                      placeholderRegistry.getBrush("deferred brush"),
              "tileset placeholder was not rebound to the real brush");
    }

    auto *groundBrush = findBrush(registry, "cave", MapBrushType::Ground);
    auto *seaBrush = findBrush(registry, "sea", MapBrushType::Ground);
    auto *sandBrush = findBrush(registry, "sand", MapBrushType::Ground);
    auto *doodadBrush = findBrush(registry, "waterfall", MapBrushType::Doodad);
    auto *wallBrush = findBrush(registry, "grass wall", MapBrushType::Wall);
    auto *doorWallBrush = findBrush(registry, "stone wall", MapBrushType::Wall);
    auto *mossyWallBrush = findBrush(registry, "mossy wall", MapBrushType::Wall);
    auto *carpetBrush = findBrush(registry, "red carpet", MapBrushType::Carpet);
    auto *tableBrush = findBrush(registry, "damaged mast2", MapBrushType::Table);
    auto *earthSoftBrush = registry.getBrush("earth");
    auto *earthHardBrush = registry.getBrush("earth (stone border)");

    require(groundBrush != nullptr, "ground brush lookup failed");
    require(seaBrush != nullptr, "sea ground brush lookup failed");
    require(sandBrush != nullptr, "sand ground brush lookup failed");
    require(doodadBrush != nullptr, "doodad brush lookup failed");
    require(wallBrush != nullptr, "wall brush lookup failed");
    require(doorWallBrush != nullptr, "door wall brush lookup failed");
    require(mossyWallBrush != nullptr, "mossy wall brush lookup failed");
    require(carpetBrush != nullptr, "carpet brush lookup failed");
    require(tableBrush != nullptr, "table brush lookup failed");
    require(earthSoftBrush != nullptr, "earth (soft) brush lookup failed");
    require(earthHardBrush != nullptr, "earth (hard) brush lookup failed");

    const auto cavePreview = groundBrush->getPreviewDescriptor();
    require(cavePreview.kind == MapEditor::Brushes::BrushPreviewKind::ServerItem &&
                cavePreview.numericId == 351,
            "cave preview descriptor should use server item lookid");
    const auto mossyWallPreview = mossyWallBrush->getPreviewDescriptor();
    require(mossyWallPreview.kind ==
                    MapEditor::Brushes::BrushPreviewKind::ServerItem &&
                mossyWallPreview.numericId == 1933,
            "mossy wall preview descriptor should use server item lookid");

    MapEditor::Domain::ChunkedMap map;
    map.createNew(128, 128, 1098);
    map.setHouseFile("brush-smoke-houses.xml");
    map.setSpawnFile("brush-smoke-spawns.xml");

    MapEditor::Domain::History::HistoryManager history;
    MapEditor::Services::BrushSettingsService settings;
    MapEditor::Services::Preview::PreviewService previewService;
    MapEditor::Services::Preview::BrushPreviewFactory previewFactory;
    MapEditor::Brushes::BrushController controller;
    controller.initialize(&map, &history, nullptr);
    controller.setBrushRegistry(&registry);
    controller.setBrushSettingsService(&settings);
    controller.setPreviewFactory(&previewFactory);
    controller.setPreviewService(&previewService);

    for (const auto *brush : {static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getSpawnBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getHouseBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getHouseExitBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getWaypointBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getOptionalBorderBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getEraserBrush()),
                              static_cast<const MapEditor::Brushes::IBrush *>(
                                  controller.getPZBrush())}) {
      const auto resolvedPreview =
          MapEditor::UI::Utils::ResolveBrushPreview(brush, nullptr, nullptr);
      require(!resolvedPreview.fallbackLabel.empty(),
              "symbolic preview fallback label should not be empty");
    }

    settings.setStandardSize(2);
    controller.setBrush(groundBrush);
    previewService.updateCursor({40, 40, 7});
    require(previewService.hasPreview(), "ground brush preview is inactive");
    require(previewService.getPreviewTiles().size() ==
                settings.getBrushOffsets().size(),
            "ground preview does not expand with brush settings");

    settings.setStandardSize(1);
    previewService.regenerate();

    const MapEditor::Domain::Position cavePos{20, 20, 7};
    require(controller.applyBrush(cavePos), "ground brush paint failed");
    auto *caveTile = map.getTile(cavePos);
    require(caveTile != nullptr && caveTile->hasGround(),
            "ground brush did not create ground");
    require(controller.selectBrushFromTile(*caveTile, PickMode::Ground),
            "ground brush reselection failed");
    require(controller.getCurrentBrush() == groundBrush,
            "ground reselection chose the wrong brush");
    controller.setBrush(wallBrush);
    const auto caveResolution =
        controller.resolveBrushFromTile(*caveTile, PickMode::Ground);
    require(caveResolution.has_value() && caveResolution->brush == groundBrush,
            "ground resolver did not identify the cave brush");
    require(controller.getCurrentBrush() == wallBrush,
            "non-mutating ground resolver changed the active brush");
    require(controller.toggleSelectionTool(),
            "spacebar-style toggle should switch to selection mode");
    require(!controller.hasBrush(),
            "selection mode toggle did not clear the active brush");
    require(controller.toggleSelectionTool(),
            "spacebar-style toggle should restore the last brush");
    require(controller.getCurrentBrush() == wallBrush,
            "selection mode toggle did not restore the previous brush");
      clearTileBrushOwnership(*caveTile);
      const auto legacyGroundResolution =
          controller.resolveBrushFromTile(*caveTile, PickMode::Ground);
      require(legacyGroundResolution.has_value() &&
                  legacyGroundResolution->brush == groundBrush,
              "legacy ground selection fallback failed");

      auto contextMap = std::make_unique<MapEditor::Domain::ChunkedMap>();
      contextMap->createNew(32, 32, 1098);
      const MapEditor::Domain::Position contextPos{6, 6, 7};
      auto contextTile = std::make_unique<MapEditor::Domain::Tile>(contextPos);
      contextTile->setGround(std::make_unique<MapEditor::Domain::Item>(101));
      auto contextTopItem = std::make_unique<MapEditor::Domain::Item>(202);
      auto *contextTopItemPtr = contextTopItem.get();
      contextTile->addItem(std::move(contextTopItem));
      contextMap->setTile(contextPos, std::move(contextTile));

      auto contextDocument =
          std::make_unique<MapEditor::Domain::MapInstance>(std::move(contextMap));
      MapEditor::AppLogic::EditorSession contextSession(
          std::move(contextDocument), 1);
      MapEditor::Domain::SelectionSettings selectionSettings;
      MapEditor::AppLogic::MapInputController inputController(
          selectionSettings, nullptr);
      MapEditor::Domain::History::HistoryManager contextHistory;
      MapEditor::Brushes::BrushController contextBrushController;
      contextBrushController.initialize(contextSession.getMap(), &contextHistory,
                                       nullptr);
      contextBrushController.setBrushRegistry(&registry);
      contextBrushController.setBrush(groundBrush);
      inputController.setBrushController(&contextBrushController);

      inputController.onRightClick(contextPos, 0, {0.0f, 0.0f}, &contextSession);
      require(!contextBrushController.hasBrush(),
              "right click should toggle out of brush mode before opening context");
      const auto contextEntries =
          contextSession.getSelectionService().getEntriesAt(contextPos);
      require(contextEntries.size() == 1 &&
                  contextEntries.front().getType() ==
                      MapEditor::Domain::Selection::EntityType::Item &&
                  contextEntries.front().entity_ptr == contextTopItemPtr,
              "right click did not select the top item as context");

      contextSession.getSelectionService().selectTile(contextSession.getMap(),
                                                      contextPos);
      const auto preservedCount =
          contextSession.getSelectionService().getEntriesAt(contextPos).size();
      inputController.onRightClick(contextPos, 0, {0.0f, 0.0f}, &contextSession);
      require(contextSession.getSelectionService().getEntriesAt(contextPos).size() ==
                  preservedCount,
              "right click should preserve existing selection on the clicked tile");

      const MapEditor::Domain::Position emptyContextPos{7, 7, 7};
      inputController.onRightClick(emptyContextPos, 0, {0.0f, 0.0f},
                                   &contextSession);
      require(contextSession.getSelectionService().isEmpty(),
              "right click on empty space should clear stale selection context");

      const MapEditor::Domain::Position seaPos{20, 22, 7};
    const MapEditor::Domain::Position sandPos{21, 22, 7};
    controller.setBrush(seaBrush);
    require(controller.applyBrush(seaPos), "sea ground paint failed");
    controller.setBrush(sandBrush);
    require(controller.applyBrush(sandPos), "sand ground paint failed");
    const auto *seaTile = map.getTile(seaPos);
    const auto *sandTile = map.getTile(sandPos);
    require(seaTile != nullptr && sandTile != nullptr, "terrain tiles were not created");
    require(seaTile->getItemCount() > 0 || sandTile->getItemCount() > 0,
            "terrain/autoborder interaction did not create border items");

    const MapEditor::Domain::Position optionalPos{22, 20, 7};
    auto *optionalGroundBrush =
        findBrush(registry, "wooden floor", MapBrushType::Ground);
    require(optionalGroundBrush != nullptr,
            "optional-border ground brush lookup failed");
    controller.setBrush(optionalGroundBrush);
    require(controller.applyBrush(optionalPos),
            "optional-border ground paint failed");
    auto *optionalTile = map.getTile(optionalPos);
    require(optionalTile != nullptr && optionalTile->hasGround(),
            "optional-border test tile has no ground");
    controller.activateOptionalBorderBrush();
    require(controller.applyBrush(optionalPos),
            "optional border brush application failed");
    require(optionalTile->hasOptionalBorder(),
            "optional border state was not stored on tile");
    require(controller.selectBrushFromTile(*optionalTile,
                                           PickMode::OptionalBorder),
            "optional border reselection failed");

    const MapEditor::Domain::Position rawPos{24, 20, 7};
    auto *rawBrush = registry.getOrCreateRAWBrush(2160);
    require(rawBrush != nullptr, "raw brush creation failed");
    controller.setBrush(rawBrush);
    require(controller.applyBrush(rawPos), "raw brush paint failed");
    auto *rawTile = map.getTile(rawPos);
    require(rawTile != nullptr && rawTile->getItemCount() > 0,
            "raw brush did not place an item");
    require(controller.selectBrushFromTile(*rawTile, PickMode::Raw),
            "raw brush reselection failed");
    require(controller.getCurrentItemId().has_value() &&
                controller.getCurrentItemId().value() == 2160,
            "raw brush reselection did not preserve the item id");
    require(controller.resolveBrushFromTile(*rawTile, PickMode::Wall) ==
                std::nullopt,
            "raw tile should not resolve as a wall brush");

    const auto verifyEarthBorder =
        [&](const MapEditor::Domain::Position &center,
            std::initializer_list<MapEditor::Domain::Position> softNeighbors,
            uint16_t expectedItemId, uint16_t wrongItemId,
            std::string_view label) {
          controller.setBrush(earthHardBrush);
          require(controller.applyBrush(center), "earth (hard) paint failed");
          controller.setBrush(earthSoftBrush);
          for (const auto &neighborPos : softNeighbors) {
            require(controller.applyBrush(neighborPos),
                    "earth (soft) paint failed");
          }

          const auto *tile = map.getTile(center);
          require(tile != nullptr, "earth parity tile missing");
          require(tileContainsItemId(*tile, expectedItemId),
                  std::string(label).append(" expected border item missing"));
          require(!tileContainsItemId(*tile, wrongItemId),
                  std::string(label)
                      .append(" wrong-direction border item present"));
        };

    verifyEarthBorder({60, 20, 7}, {{60, 19, 7}}, 5632, 5637,
                      "earth north edge");
    verifyEarthBorder({64, 20, 7}, {{65, 20, 7}}, 5637, 5632,
                      "earth east edge");
    verifyEarthBorder({66, 24, 7}, {{66, 25, 7}}, 5638, 5631,
                      "earth south edge");
    verifyEarthBorder({62, 24, 7}, {{61, 24, 7}}, 5631, 5638,
                      "earth west edge");
    verifyEarthBorder({68, 20, 7}, {{68, 19, 7}, {69, 20, 7}}, 5651, 5647,
                      "earth north-east diagonal");
    verifyEarthBorder({72, 20, 7}, {{72, 19, 7}, {71, 20, 7}}, 5647, 5651,
                      "earth north-west diagonal");
    verifyEarthBorder({68, 24, 7}, {{69, 24, 7}, {68, 25, 7}}, 5649, 5650,
                      "earth south-east diagonal");
    verifyEarthBorder({72, 24, 7}, {{71, 24, 7}, {72, 25, 7}}, 5650, 5649,
                      "earth south-west diagonal");

    auto doodadProvider = previewFactory.createProvider(doodadBrush, &settings);
    require(doodadProvider != nullptr && doodadProvider->isActive(),
            "doodad preview provider was not created");
    doodadProvider->updateCursorPosition({50, 20, 7});
    const auto &doodadPreviewTiles = doodadProvider->getTiles();
    require(!doodadPreviewTiles.empty(), "doodad preview is empty");
    require(doodadPreviewTiles.size() > 1 ||
                doodadPreviewTiles.front().relativePosition.x != 0 ||
                doodadPreviewTiles.front().relativePosition.y != 0 ||
                doodadPreviewTiles.front().relativePosition.z != 0,
            "doodad preview did not expose a composite/offset footprint");

    const MapEditor::Domain::Position doodadPos{50, 20, 7};
    const auto doodadTileCountBefore = countTiles(map);
    controller.setBrush(doodadBrush);
    require(controller.applyBrush(doodadPos), "doodad brush paint failed");
    require(countTiles(map) >= doodadTileCountBefore,
            "doodad paint unexpectedly removed tiles");
    const auto doodadPaintedPos = findTileWithItemsNear(map, doodadPos, 2);
    require(doodadPaintedPos.has_value(),
            "doodad placement did not affect the map");
    auto *doodadTile = map.getTile(*doodadPaintedPos);
    require(doodadTile != nullptr, "doodad tile lookup failed");
    require(controller.selectBrushFromTile(*doodadTile, PickMode::Doodad),
            "doodad brush reselection failed");
    clearTileBrushOwnership(*doodadTile);
    require(controller.resolveBrushFromTile(*doodadTile, PickMode::Doodad)
                .has_value(),
            "legacy doodad selection fallback failed");

    const MapEditor::Domain::Position wallLeftPos{30, 20, 7};
    const MapEditor::Domain::Position wallCenterPos{31, 20, 7};
    const MapEditor::Domain::Position wallRightPos{32, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(wallLeftPos),
            "left wall ground paint failed");
    controller.setBrush(groundBrush);
    require(controller.applyBrush(wallCenterPos),
            "center wall ground paint failed");
    controller.setBrush(groundBrush);
    require(controller.applyBrush(wallRightPos),
            "right wall ground paint failed");
    controller.setBrush(doorWallBrush);
    require(controller.applyBrush(wallLeftPos), "left wall brush paint failed");
    require(controller.applyBrush(wallCenterPos),
            "center wall brush paint failed");
    require(controller.applyBrush(wallRightPos),
            "right wall brush paint failed");
    auto *wallTile = map.getTile(wallCenterPos);
    require(wallTile != nullptr && wallTile->getItemCount() > 0,
            "wall brush did not place wall items");
    const auto wallItemBeforeDoor =
        wallTile->getItem(wallTile->getItemCount() - 1)->getServerId();
    controller.setBrush(doorWallBrush);
    require(controller.applyBrush(wallCenterPos),
            "single-tile wall special click failed");
    const auto wallItemAfterSpecialClick =
        wallTile->getItem(wallTile->getItemCount() - 1)->getServerId();
    require(wallItemAfterSpecialClick != wallItemBeforeDoor,
            "single-tile wall special click did not cycle the wall variant");
    controller.activateMagicDoorBrush();
    require(controller.applyBrush(wallCenterPos), "door brush paint failed");
    require(wallTile->getItemCount() > 0,
            "door brush left the wall tile without any wall items");
    const auto wallItemAfterDoor =
        wallTile->getItem(wallTile->getItemCount() - 1)->getServerId();
    require(wallItemAfterDoor != wallItemBeforeDoor,
            "door brush did not alter the wall tile contents");
    require(tileContainsItemId(*wallTile, 6265),
            "door brush did not place the expected magic door item");
    require(controller.canSwitchDoorAt(wallCenterPos),
            "switch door controller did not identify the placed door");
    require(controller.switchDoorAt(wallCenterPos), "switch door controller failed");
    require(!tileContainsItemId(*wallTile, 6265),
            "switch door controller did not change the active door variant");
    require(controller.selectBrushFromTile(*wallTile, PickMode::Door),
            "door brush reselection failed");
    require(controller.getCurrentBrush() != nullptr &&
                controller.getCurrentBrush()->getType() == MapBrushType::Door,
            "door reselection did not activate a door brush");
    clearTileBrushOwnership(*wallTile);
    require(controller.resolveBrushFromTile(*wallTile, PickMode::Door)
                .has_value(),
            "legacy door selection fallback failed");
    require(controller.eraseBrush(wallCenterPos), "door brush erase failed");
    require(!tileContainsItemId(*wallTile, 6265),
            "door brush erase left the magic door item behind");
    require(tileContainsItemId(*wallTile, wallItemBeforeDoor),
            "door brush erase did not restore the wallized base wall item");
    require(!controller.resolveBrushFromTile(*wallTile, PickMode::Door)
                 .has_value(),
            "door selection fallback still identified a removed door");

    const MapEditor::Domain::Position decoLeftPos{44, 24, 7};
    const MapEditor::Domain::Position decoCenterPos{45, 24, 7};
    const MapEditor::Domain::Position decoRightPos{46, 24, 7};
    for (const auto &pos :
         std::array{decoLeftPos, decoCenterPos, decoRightPos}) {
      controller.setBrush(groundBrush);
      require(controller.applyBrush(pos),
              "wall decoration support ground paint failed");
      controller.setBrush(doorWallBrush);
      require(controller.applyBrush(pos), "wall decoration base wall paint failed");
    }

    controller.setBrush(mossyWallBrush);
    require(controller.applyBrush(decoCenterPos),
            "wall decoration brush paint failed");
    auto *decoTile = map.getTile(decoCenterPos);
    require(decoTile != nullptr && decoTile->getItemCount() > 1,
            "wall decoration brush did not stack over the base wall");
    const auto decorationCountBeforeRebuild =
        countOwnedItems(*decoTile, *mossyWallBrush);
    require(decorationCountBeforeRebuild > 0,
            "wall decoration brush did not place an owned decoration item");

    controller.setBrush(doorWallBrush);
    require(controller.eraseBrush(decoLeftPos),
            "wall decoration neighbor erase failed");
    const auto decorationCountAfterRebuild =
        countOwnedItems(*decoTile, *mossyWallBrush);
    require(decorationCountAfterRebuild == decorationCountBeforeRebuild,
            "wall rebuild collapsed the stacked wall decoration");

    const std::array normalDoorCenters{
        MapEditor::Domain::Position{47, 20, 7},
        MapEditor::Domain::Position{51, 20, 7},
    };
    for (const auto &centerPos : normalDoorCenters) {
      controller.setBrush(groundBrush);
      require(controller.applyBrush({centerPos.x - 1, centerPos.y, centerPos.z}),
              "normal door left support ground paint failed");
      controller.setBrush(groundBrush);
      require(controller.applyBrush(centerPos),
              "normal door center ground paint failed");
      controller.setBrush(groundBrush);
      require(controller.applyBrush({centerPos.x + 1, centerPos.y, centerPos.z}),
              "normal door right support ground paint failed");
      controller.setBrush(doorWallBrush);
      require(controller.applyBrush({centerPos.x - 1, centerPos.y, centerPos.z}),
              "normal door left wall paint failed");
      require(controller.applyBrush(centerPos), "normal door center wall paint failed");
      require(controller.applyBrush({centerPos.x + 1, centerPos.y, centerPos.z}),
              "normal door right wall paint failed");
    }

    controller.activateNormalDoorBrush();
    require(controller.applyBrush(normalDoorCenters[0]),
            "normal door paint without shift failed");
    const auto *normalDoorTile = map.getTile(normalDoorCenters[0]);
    require(normalDoorTile != nullptr && normalDoorTile->getItemCount() > 0,
            "normal door tile lookup failed");
    const auto normalDoorId =
        normalDoorTile->getItem(normalDoorTile->getItemCount() - 1)->getServerId();

    controller.activateLockedDoorBrush();
    require(controller.applyBrush(normalDoorCenters[1]),
            "locked door paint failed");
    const auto *lockedDoorTile = map.getTile(normalDoorCenters[1]);
    require(lockedDoorTile != nullptr && lockedDoorTile->getItemCount() > 0,
            "locked door tile lookup failed");
    const auto lockedDoorId =
        lockedDoorTile->getItem(lockedDoorTile->getItemCount() - 1)->getServerId();
    require(normalDoorId != lockedDoorId,
            "locked door brush did not choose a different variant");

    const MapEditor::Domain::Position carpetPos{34, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(carpetPos), "carpet test ground paint failed");
    controller.setBrush(carpetBrush);
    require(controller.applyBrush(carpetPos), "carpet brush paint failed");
    require(map.getTile(carpetPos) != nullptr &&
                map.getTile(carpetPos)->getItemCount() > 0,
            "carpet brush did not place aligned items");
    clearTileBrushOwnership(*map.getTile(carpetPos));
    require(controller.resolveBrushFromTile(*map.getTile(carpetPos),
                                            PickMode::Carpet)
                .has_value(),
            "legacy carpet selection fallback failed");

    const MapEditor::Domain::Position tableSupportPos{36, 20, 7};
    const MapEditor::Domain::Position tablePos{37, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(tableSupportPos),
            "table support ground paint failed");
    controller.setBrush(groundBrush);
    require(controller.applyBrush(tablePos), "table test ground paint failed");
    controller.setBrush(tableBrush);
    require(controller.applyBrush(tableSupportPos),
            "table support brush paint failed");
    require(controller.applyBrush(tablePos), "table brush paint failed");
    require(map.getTile(tableSupportPos) != nullptr &&
                map.getTile(tableSupportPos)->getItemCount() > 0,
            "table support brush did not place aligned items");
    require(map.getTile(tablePos) != nullptr &&
                map.getTile(tablePos)->getItemCount() > 0,
            "table brush did not place aligned items");
    require(controller.selectBrushFromTile(*map.getTile(tablePos), PickMode::Table),
            "table brush reselection failed");
    clearTileBrushOwnership(*map.getTile(tablePos));
    require(controller.resolveBrushFromTile(*map.getTile(tablePos),
                                            PickMode::Table)
                .has_value(),
            "legacy table selection fallback failed");

    const MapEditor::Domain::Position rotatePos{38, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(rotatePos), "rotate test ground paint failed");
    auto *rotatableRawBrush =
        dynamic_cast<MapEditor::Brushes::RawBrush *>(registry.getOrCreateRAWBrush(1650));
    require(rotatableRawBrush != nullptr, "rotatable raw brush creation failed");
    MapEditor::Domain::ItemType rotatableType;
    rotatableType.server_id = 1650;
    rotatableType.client_id = 1650;
    rotatableType.rotateTo = 1651;
    rotatableRawBrush->setCachedType(&rotatableType);
    controller.setBrush(rotatableRawBrush);
    require(controller.applyBrush(rotatePos), "rotatable raw brush paint failed");
    auto *rotateTile = map.getTile(rotatePos);
    require(rotateTile != nullptr && tileContainsItemId(*rotateTile, 1650),
            "rotatable raw brush did not place the expected item");
    require(controller.canRotateItemAt(rotatePos),
            "rotate controller did not identify rotatable item");
    require(controller.rotateItemAt(rotatePos), "rotate controller failed");
    require(tileContainsItemId(*rotateTile, 1651),
            "rotate controller did not advance item to rotateTo target");

    MapEditor::Services::Selection::SelectionService selection;
    selection.selectTile(&map, cavePos);
    require(!selection.getEntriesAt(cavePos).empty(),
            "selection did not include the painted tile entries");
    selection.selectTile(&map, rawPos);
    require(selection.getPositions().size() >= 2,
            "selection did not preserve multi-tile selection");

    const MapEditor::Domain::Position housePos{40, 20, 7};
    controller.getHouseBrush()->setHouseId(42);
    controller.activateHouseBrush();
    require(controller.applyBrush(housePos), "house brush paint failed");
    auto *houseTile = map.getTile(housePos);
    require(houseTile != nullptr && houseTile->getHouseId() == 42,
            "house brush did not assign the house id");
    require(map.getHouse(42) != nullptr, "house metadata was not created");
    require(controller.selectBrushFromTile(*houseTile, PickMode::House),
            "house brush reselection failed");
    require(controller.getHouseBrush()->getHouseId() == 42,
            "house brush reselection lost the selected house id");

    const MapEditor::Domain::Position houseExitPos{41, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(houseExitPos),
            "house exit ground paint failed");
    controller.getHouseExitBrush()->setHouseId(42);
    controller.activateHouseExitBrush();
    require(controller.applyBrush(houseExitPos), "house exit paint failed");
    require(map.getHouse(42)->entry_position == houseExitPos,
            "house exit position was not stored");
    auto *houseExitTile = map.getTile(houseExitPos);
    require(houseExitTile != nullptr, "house exit tile does not exist");
    require(controller.selectBrushFromTile(*houseExitTile, PickMode::HouseExit),
            "house exit brush reselection failed");
    require(controller.getHouseExitBrush()->getHouseId() == 42,
            "house exit reselection lost the owning house id");

    const MapEditor::Domain::Position waypointPos{42, 20, 7};
    controller.getWaypointBrush()->setWaypointName("smoke-waypoint");
    controller.activateWaypointBrush();
    require(controller.applyBrush(waypointPos), "waypoint brush paint failed");
    require(map.getWaypointAt(waypointPos) != nullptr,
            "waypoint metadata was not stored");
    auto *waypointTile = map.getTile(waypointPos);
    require(waypointTile != nullptr, "waypoint tile does not exist");
    require(controller.selectBrushFromTile(*waypointTile, PickMode::Waypoint),
            "waypoint brush reselection failed");
    require(controller.getWaypointBrush()->getWaypointName() == "smoke-waypoint",
            "waypoint reselection lost the waypoint name");

    const MapEditor::Domain::Position spawnPos{43, 20, 7};
    controller.activateSpawnBrush();
    require(controller.applyBrush(spawnPos), "spawn brush paint failed");
    auto *spawnTile = map.getTile(spawnPos);
    require(spawnTile != nullptr && spawnTile->hasSpawn(),
            "spawn metadata was not stored");
    require(controller.selectBrushFromTile(*spawnTile, PickMode::Spawn),
            "spawn brush reselection failed");
    require(controller.getCurrentBrush() != nullptr &&
                controller.getCurrentBrush()->getType() == MapBrushType::Spawn,
            "spawn reselection did not activate the spawn brush");

    const MapEditor::Domain::Position zonePos{44, 20, 7};
    controller.activatePZBrush();
    require(controller.applyBrush(zonePos), "zone brush paint failed");
    auto *zoneTile = map.getTile(zonePos);
    require(zoneTile != nullptr &&
                zoneTile->hasFlag(MapEditor::Domain::TileFlag::ProtectionZone),
            "zone flag was not stored");
    require(controller.selectBrushFromTile(*zoneTile, PickMode::ProtectionZone),
            "zone brush reselection failed");
    clearTileBrushOwnership(*zoneTile);
    require(controller.resolveBrushFromTile(*zoneTile, PickMode::ProtectionZone)
                .has_value(),
            "legacy zone selection fallback failed");

    const MapEditor::Domain::Position erasePos{45, 20, 7};
    controller.setBrush(groundBrush);
    require(controller.applyBrush(erasePos), "eraser test ground paint failed");
    controller.setBrush(rawBrush);
    require(controller.applyBrush(erasePos), "eraser test raw paint failed");
    controller.activateSpawnBrush();
    require(controller.applyBrush(erasePos), "eraser test spawn paint failed");
    controller.activatePZBrush();
    require(controller.applyBrush(erasePos), "eraser test zone paint failed");
    controller.getWaypointBrush()->setWaypointName("erase-me");
    controller.activateWaypointBrush();
    require(controller.applyBrush(erasePos), "eraser test waypoint paint failed");
    controller.getHouseBrush()->setHouseId(99);
    controller.activateHouseBrush();
    require(controller.applyBrush(erasePos), "eraser test house paint failed");
    auto *eraseTile = map.getTile(erasePos);
    require(eraseTile != nullptr, "eraser test tile missing");
    controller.activateEraserBrush();
    require(controller.applyBrush(erasePos), "eraser brush paint failed");
    require(eraseTile->getGround() == nullptr && eraseTile->getItemCount() == 0 &&
                !eraseTile->hasSpawn() &&
                !eraseTile->hasFlag(MapEditor::Domain::TileFlag::ProtectionZone) &&
                eraseTile->getHouseId() == 0 &&
                map.getWaypointAt(erasePos) == nullptr,
            "eraser brush did not clear painted tile state");
    require(controller.resolveBrushFromTile(*eraseTile, PickMode::Smart) ==
                std::nullopt,
            "erased tile should not resolve to a brush");

    const fs::path housesXml = tempDir / "houses.xml";
    require(MapEditor::IO::HouseXmlWriter::write(housesXml, map),
            "houses.xml write failed");
    MapEditor::Domain::ChunkedMap houseRoundTrip;
    const auto houseRead = MapEditor::IO::HouseXmlReader::read(housesXml, houseRoundTrip);
    require(houseRead.success, "houses.xml read failed");
    const auto *roundTripHouse = houseRoundTrip.getHouse(42);
    require(roundTripHouse != nullptr &&
                roundTripHouse->entry_position == houseExitPos,
            "house entry position was not preserved");

    const fs::path spawnsXml = tempDir / "spawns.xml";
    require(MapEditor::IO::SpawnXmlWriter::write(spawnsXml, map),
            "spawns.xml write failed");
    MapEditor::Domain::ChunkedMap spawnRoundTrip;
    const auto spawnRead = MapEditor::IO::SpawnXmlReader::read(spawnsXml, spawnRoundTrip);
    require(spawnRead.success, "spawns.xml read failed");
    const auto *roundTripSpawnTile = spawnRoundTrip.getTile(spawnPos);
    require(roundTripSpawnTile != nullptr && roundTripSpawnTile->hasSpawn(),
            "spawn state was not preserved");

    const fs::path otbmPath = tempDir / "brush-smoke.otbm";
    const auto writeResult = MapEditor::IO::OtbmWriter::write(otbmPath, map);
    require(writeResult.success, "OTBM write failed");
    const auto readResult = MapEditor::IO::OtbmReader::read(otbmPath);
    require(readResult.success && readResult.map != nullptr,
            "OTBM read failed");
    const auto *roundTripWaypoint = readResult.map->getWaypointAt(waypointPos);
    require(roundTripWaypoint != nullptr &&
                roundTripWaypoint->name == "smoke-waypoint",
            "waypoint OTBM round-trip failed");
    const auto *roundTripHouseTile = readResult.map->getTile(housePos);
    require(roundTripHouseTile != nullptr &&
                roundTripHouseTile->getHouseId() == 42,
            "house tile OTBM round-trip failed");

    fs::remove_all(tempDir, cleanupError);
    std::cout << "Brush smoke OK" << std::endl;
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "Brush smoke failed: " << ex.what() << std::endl;
    return 1;
  }
}
