#include "BrushPreviewFactory.h"
#include "MultiItemBrushPreviewProvider.h"
#include "PreviewTypes.h"
#include "Brushes/Types/CarpetBrush.h"
#include "Brushes/Types/CreatureBrush.h"
#include "Brushes/Types/DoodadBrush.h"
#include "Brushes/Types/DoorBrush.h"
#include "Brushes/Types/EraserBrush.h"
#include "Brushes/Types/FlagBrush.h"
#include "Brushes/Types/GroundBrush.h"
#include "Brushes/Types/HouseBrush.h"
#include "Brushes/Types/HouseExitBrush.h"
#include "Brushes/Types/OptionalBorderBrush.h"
#include "Brushes/Types/RawBrush.h"
#include "Brushes/Types/SpawnBrush.h"
#include "Brushes/Types/TableBrush.h"
#include "Brushes/Types/WaypointBrush.h"
#include "Brushes/Types/WallBrush.h"
#include "CreaturePreviewProvider.h"
#include "DoodadBrushPreviewProvider.h"
#include "IPreviewProvider.h"
#include "RawBrushPreviewProvider.h"
#include "SpawnPreviewProvider.h"
#include "ZoneBrushPreviewProvider.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Services::Preview {

std::unique_ptr<IPreviewProvider>
BrushPreviewFactory::createProvider(const Brushes::IBrush *brush,
                                    BrushSettingsService *settings) {
  if (!brush) {
    return nullptr;
  }

  // RAW Brush -> RawBrushPreviewProvider
  if (auto *rawBrush = dynamic_cast<const Brushes::RawBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating RawBrushPreviewProvider for "
                  "item {}",
                  rawBrush->getItemId());
    return std::make_unique<RawBrushPreviewProvider>(rawBrush->getItemId(), 0,
                                                     settings);
  }

  // Creature Brush -> CreaturePreviewProvider
  if (auto *creatureBrush =
          dynamic_cast<const Brushes::CreatureBrush *>(brush)) {
    spdlog::debug(
        "[BrushPreviewFactory] Creating CreaturePreviewProvider for: {}",
        creatureBrush->getName());
    return std::make_unique<CreaturePreviewProvider>(creatureBrush->getName(),
                                                     settings);
  }

  // Spawn Brush -> SpawnPreviewProvider (shows radius circle)
  if (auto *spawnBrush = dynamic_cast<const Brushes::SpawnBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating SpawnPreviewProvider");
    return std::make_unique<SpawnPreviewProvider>(settings);
  }

  auto makeSingleItemPreview =
      [settings](uint16_t itemId) -> std::unique_ptr<IPreviewProvider> {
    if (itemId == 0) {
      return std::unique_ptr<IPreviewProvider>{};
    }
    PreviewTileData tile(0, 0, 0);
    tile.addItem(itemId);
    std::vector<PreviewTileData> tiles;
    tiles.push_back(std::move(tile));
    return std::make_unique<MultiItemBrushPreviewProvider>(std::move(tiles),
                                                           settings, true);
  };

  if (auto *groundBrush = dynamic_cast<const Brushes::GroundBrush *>(brush)) {
    return makeSingleItemPreview(groundBrush->getPreviewItemId());
  }

  if (auto *wallBrush = dynamic_cast<const Brushes::WallBrush *>(brush)) {
    return makeSingleItemPreview(wallBrush->getPreviewItemId());
  }

  if (auto *carpetBrush = dynamic_cast<const Brushes::CarpetBrush *>(brush)) {
    return makeSingleItemPreview(carpetBrush->getPreviewItemId());
  }

  if (auto *tableBrush = dynamic_cast<const Brushes::TableBrush *>(brush)) {
    return makeSingleItemPreview(tableBrush->getPreviewItemId());
  }

  if (auto *doorBrush = dynamic_cast<const Brushes::DoorBrush *>(brush)) {
    return makeSingleItemPreview(static_cast<uint16_t>(doorBrush->getLookId()));
  }

  if (auto *doodadBrush = dynamic_cast<const Brushes::DoodadBrush *>(brush)) {
    return std::make_unique<DoodadBrushPreviewProvider>(*doodadBrush, settings);
  }

  if (auto *optionalBrush =
          dynamic_cast<const Brushes::OptionalBorderBrush *>(brush)) {
    (void)optionalBrush;
    return std::make_unique<ZoneBrushPreviewProvider>(0x80D7C45A, settings);
  }

  if (auto *houseExitBrush =
          dynamic_cast<const Brushes::HouseExitBrush *>(brush)) {
    (void)houseExitBrush;
    return std::make_unique<ZoneBrushPreviewProvider>(0x80FFAA44, settings);
  }

  // Flag Brush -> ZoneBrushPreviewProvider (yellow for zone flags)
  if (auto *flagBrush = dynamic_cast<const Brushes::FlagBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating ZoneBrushPreviewProvider for "
                  "FlagBrush");
    return std::make_unique<ZoneBrushPreviewProvider>(
        0x80FFFF00, settings); // Semi-transparent yellow
  }

  // Eraser Brush -> ZoneBrushPreviewProvider (red for eraser)
  if (auto *eraserBrush = dynamic_cast<const Brushes::EraserBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating ZoneBrushPreviewProvider for "
                  "EraserBrush");
    return std::make_unique<ZoneBrushPreviewProvider>(
        0x80FF4444, settings); // Semi-transparent red
  }

  // House Brush -> ZoneBrushPreviewProvider (blue for houses)
  if (auto *houseBrush = dynamic_cast<const Brushes::HouseBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating ZoneBrushPreviewProvider for "
                  "HouseBrush");
    return std::make_unique<ZoneBrushPreviewProvider>(
        0x804488FF, settings); // Semi-transparent blue
  }

  // Waypoint Brush -> ZoneBrushPreviewProvider (green for waypoints)
  if (auto *waypointBrush =
          dynamic_cast<const Brushes::WaypointBrush *>(brush)) {
    spdlog::debug("[BrushPreviewFactory] Creating ZoneBrushPreviewProvider for "
                  "WaypointBrush");
    return std::make_unique<ZoneBrushPreviewProvider>(
        0x8044FF44, settings); // Semi-transparent green
  }

  // Unknown brush type - no preview
  spdlog::debug("[BrushPreviewFactory] No preview provider for brush type: {}",
                brush->getName());
  return nullptr;
}

} // namespace MapEditor::Services::Preview
