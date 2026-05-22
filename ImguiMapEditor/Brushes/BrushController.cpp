#include "BrushController.h"
#include "BrushRegistry.h"
#include "Domain/Item.h"
#include "Services/BrushSettingsService.h"
#include "Services/ClientDataService.h"
#include "Services/Preview/BrushPreviewFactory.h"
#include "Services/Preview/PreviewService.h"
#include "Types/GroundBrush.h"
#include "Types/DoodadBrush.h"
#include "Types/WallBrush.h"
#include "Types/DoorBrush.h"
#include "Types/RawBrush.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

namespace {

bool matchesBrushType(const IBrush *brush, BrushType type) {
  if (!brush) {
    return false;
  }

  const auto brushType = brush->getType();
  if (type == BrushType::Wall) {
    return brushType == BrushType::Wall ||
           brushType == BrushType::WallDecoration;
  }

  return brushType == type;
}

const Domain::Item *getTopTileItem(const Domain::Tile &tile) {
  if (tile.getItemCount() == 0) {
    return nullptr;
  }
  return tile.getItem(tile.getItemCount() - 1);
}

IBrush *findItemBrushByType(const Domain::Tile &tile, BrushRegistry *registry,
                            BrushType type, bool collectionOnly = false) {
  if (!registry) {
    return nullptr;
  }

  for (size_t index = tile.getItemCount(); index > 0; --index) {
    const auto *item = tile.getItem(index - 1);
    if (!item) {
      continue;
    }

    for (auto *brush : registry->getBrushesForItem(item->getServerId())) {
      if (!matchesBrushType(brush, type)) {
        continue;
      }
      if (collectionOnly && (!brush->visibleInPalette() || !brush->hasCollection())) {
        continue;
      }
      if (brush) {
        return brush;
      }
    }
  }

  return nullptr;
}

IBrush *findOwnedItemBrushByType(const Domain::Tile &tile, BrushRegistry *registry,
                                 BrushType type, bool collectionOnly = false) {
  if (!registry) {
    return nullptr;
  }

  for (size_t index = tile.getItemCount(); index > 0; --index) {
    const auto *item = tile.getItem(index - 1);
    if (!item || item->getOwnerBrushId() == InvalidBrushId) {
      continue;
    }

    if (auto *brush = registry->getBrushById(item->getOwnerBrushId());
        matchesBrushType(brush, type)) {
      if (!collectionOnly || (brush->visibleInPalette() && brush->hasCollection())) {
        return brush;
      }
    }
  }

  if (const auto *ground = tile.getGround();
      ground && ground->getOwnerBrushId() != InvalidBrushId) {
    if (auto *brush = registry->getBrushById(ground->getOwnerBrushId());
        matchesBrushType(brush, type)) {
      if (!collectionOnly || (brush->visibleInPalette() && brush->hasCollection())) {
        return brush;
      }
    }
  }

  return nullptr;
}

DoorType getDoorTypeForTile(const Domain::Tile &tile, BrushRegistry *registry) {
  if (!registry) {
    return DoorType::Undefined;
  }

  for (size_t index = tile.getItemCount(); index > 0; --index) {
    const auto *item = tile.getItem(index - 1);
    if (!item) {
      continue;
    }

    for (auto *brush : registry->getBrushesForItem(item->getServerId())) {
      auto *wallBrush = dynamic_cast<WallBrush *>(brush);
      if (!wallBrush) {
        continue;
      }

      if (auto door = wallBrush->findDoorForItem(item->getServerId())) {
        return door->type;
      }
    }
  }

  return DoorType::Undefined;
}

DoorType getDoorTypeForItem(const Domain::Item &item, BrushRegistry *registry) {
  if (!registry) {
    return DoorType::Undefined;
  }

  for (auto *brush : registry->getBrushesForItem(item.getServerId())) {
    auto *wallBrush = dynamic_cast<WallBrush *>(brush);
    if (!wallBrush) {
      continue;
    }

    if (auto door = wallBrush->findDoorForItem(item.getServerId())) {
      return door->type;
    }
  }

  return DoorType::Undefined;
}

struct ResolvedDoorTarget {
  WallBrush *wallBrush = nullptr;
  Domain::Item *item = nullptr;
};

Domain::Item *resolveMutableTileItem(Domain::Tile *tile,
                                     const Domain::Item *preferredItem) {
  if (!tile) {
    return nullptr;
  }

  if (preferredItem) {
    for (auto &item : tile->getItems()) {
      if (item.get() == preferredItem) {
        return item.get();
      }
    }
  }

  for (auto it = tile->getItems().rbegin(); it != tile->getItems().rend(); ++it) {
    if (*it) {
      return it->get();
    }
  }

  return nullptr;
}

ResolvedDoorTarget resolveDoorTarget(const Domain::Tile *tile, BrushRegistry *registry,
                                     const Domain::Item *preferredItem) {
  if (!tile || !registry) {
    return {};
  }

  const auto resolveItem = [registry](Domain::Item *item) -> WallBrush * {
    if (!item) {
      return nullptr;
    }

    for (auto *brush : registry->getBrushesForItem(item->getServerId())) {
      auto *wallBrush = dynamic_cast<WallBrush *>(brush);
      if (wallBrush && wallBrush->findDoorForItem(item->getServerId()).has_value()) {
        return wallBrush;
      }
    }

    return nullptr;
  };

  if (preferredItem) {
    for (auto &item : tile->getItems()) {
      if (item.get() != preferredItem) {
        continue;
      }

      if (auto *wallBrush = resolveItem(item.get())) {
        return {.wallBrush = wallBrush, .item = item.get()};
      }
      break;
    }
  }

  for (auto it = tile->getItems().rbegin(); it != tile->getItems().rend(); ++it) {
    if (!*it) {
      continue;
    }

    if (auto *wallBrush = resolveItem(it->get())) {
      return {.wallBrush = wallBrush, .item = it->get()};
    }
  }

  return {};
}

} // namespace

BrushController::~BrushController() = default;

void BrushController::initialize(
    Domain::ChunkedMap *map, Domain::History::HistoryManager *historyManager,
    Services::ClientDataService *clientData) {
  map_ = map;
  historyManager_ = historyManager;
  clientData_ = clientData;
  spdlog::debug("[BrushController] Initialized with map, history manager, and "
                "client data");
}

void BrushController::setBrushRegistry(BrushRegistry *registry) {
  registry_ = registry;
  if (!registry_) {
    normalDoorBrush_.reset();
    normalAltDoorBrush_.reset();
    lockedDoorBrush_.reset();
    questDoorBrush_.reset();
    magicDoorBrush_.reset();
    archwayBrush_.reset();
    windowBrush_.reset();
    hatchWindowBrush_.reset();
    return;
  }

  registry_->registerExternalBrush(&spawnBrush_);
  registry_->registerExternalBrush(&pzBrush_);
  registry_->registerExternalBrush(&noPvpBrush_);
  registry_->registerExternalBrush(&noLogoutBrush_);
  registry_->registerExternalBrush(&pvpZoneBrush_);
  registry_->registerExternalBrush(&eraserBrush_);
  registry_->registerExternalBrush(&houseBrush_);
  registry_->registerExternalBrush(&houseExitBrush_);
  registry_->registerExternalBrush(&waypointBrush_);
  registry_->registerExternalBrush(&optionalBorderBrush_);

  normalDoorBrush_ = std::make_unique<DoorBrush>("Normal Door", 0,
                                                 DoorType::Normal, *registry_);
  normalAltDoorBrush_ = std::make_unique<DoorBrush>(
      "Normal Alt Door", 0, DoorType::NormalAlt, *registry_);
  lockedDoorBrush_ = std::make_unique<DoorBrush>("Locked Door", 0,
                                                 DoorType::Locked, *registry_);
  questDoorBrush_ = std::make_unique<DoorBrush>("Quest Door", 0,
                                                DoorType::Quest, *registry_);
  magicDoorBrush_ = std::make_unique<DoorBrush>("Magic Door", 0,
                                                DoorType::Magic, *registry_);
  archwayBrush_ = std::make_unique<DoorBrush>("Archway", 0,
                                              DoorType::Archway, *registry_);
  windowBrush_ = std::make_unique<DoorBrush>("Window", 0,
                                             DoorType::Window, *registry_);
  hatchWindowBrush_ = std::make_unique<DoorBrush>("Hatch Window", 0,
                                                  DoorType::HatchWindow,
                                                  *registry_);
  registry_->registerExternalBrush(normalDoorBrush_.get());
  registry_->registerExternalBrush(normalAltDoorBrush_.get());
  registry_->registerExternalBrush(lockedDoorBrush_.get());
  registry_->registerExternalBrush(questDoorBrush_.get());
  registry_->registerExternalBrush(magicDoorBrush_.get());
  registry_->registerExternalBrush(archwayBrush_.get());
  registry_->registerExternalBrush(windowBrush_.get());
  registry_->registerExternalBrush(hatchWindowBrush_.get());
}

void BrushController::setBrush(IBrush *brush) {
  if (!brush) {
    clearBrush();
    return;
  }

  currentBrush_ = brush;
  currentBrushName_ = brush->getName();
  currentBrush_->setVariation(static_cast<size_t>(variation_));
  lastBrushSelection_ = captureCurrentSelection();

  // Use factory to create preview provider
  if (previewService_ && previewFactory_) {
    auto provider =
        previewFactory_->createProvider(brush, brushSettingsService_);
    if (provider) {
      previewService_->setProvider(std::move(provider));
    } else {
      previewService_->clearPreview();
    }
  } else if (previewService_) {
    // No factory available - clear preview
    previewService_->clearPreview();
    spdlog::warn("[BrushController] No preview factory available");
  }

  if (onBrushActivated_) {
    onBrushActivated_();
  }

  spdlog::info("[BrushController] Set brush: {}", brush->getName());
}

void BrushController::clearBrush() {
  if (currentBrush_) {
    lastBrushSelection_ = captureCurrentSelection();
  }

  currentBrush_ = nullptr;
  currentBrushName_.clear();

  // Clear preview
  if (previewService_) {
    previewService_->clearPreview();
  }

  spdlog::debug("[BrushController] Brush cleared");
}

bool BrushController::restoreLastBrush() {
  return lastBrushSelection_.has_value() &&
         applyResolvedSelection(*lastBrushSelection_);
}

bool BrushController::toggleSelectionTool() {
  if (hasBrush()) {
    clearBrush();
    return true;
  }

  return restoreLastBrush();
}

bool BrushController::canRotateItemAt(const Domain::Position &pos,
                                      const Domain::Item *preferredItem) const {
  if (!map_) {
    return false;
  }

  const auto *tile = map_->getTile(pos);
  const auto *item = resolveMutableTileItem(const_cast<Domain::Tile *>(tile),
                                            preferredItem);
  const auto *type = item ? item->getType() : nullptr;
  return type && type->rotateTo != 0;
}

bool BrushController::rotateItemAt(const Domain::Position &pos,
                                   const Domain::Item *preferredItem) {
  if (!map_ || !historyManager_) {
    return false;
  }

  auto *tile = map_->getTile(pos);
  auto *item = resolveMutableTileItem(tile, preferredItem);
  const auto *type = item ? item->getType() : nullptr;
  if (!tile || !item || !type || type->rotateTo == 0) {
    return false;
  }

  historyManager_->beginOperation("Rotate item",
                                  Domain::History::ActionType::Other, nullptr);
  historyManager_->recordTileBefore(pos, tile);

  const auto rotatedId = type->rotateTo;
  item->setServerId(rotatedId);
  if (clientData_) {
    if (const auto *rotatedType = clientData_->getItemTypeByServerId(rotatedId)) {
      item->setType(rotatedType);
      item->setClientId(rotatedType->client_id);
    } else {
      item->setType(nullptr);
      item->setClientId(0);
    }
  }

  tile->markDirty();
  historyManager_->endOperation(map_, nullptr);
  return true;
}

void BrushController::activateSpawnBrush() {
  setBrush(&spawnBrush_);
  spdlog::info("[BrushController] Spawn brush activated");
}

void BrushController::activateNormalDoorBrush() {
  if (normalDoorBrush_) {
    setBrush(normalDoorBrush_.get());
  }
}

void BrushController::activateNormalAltDoorBrush() {
  if (normalAltDoorBrush_) {
    setBrush(normalAltDoorBrush_.get());
  } else {
    activateNormalDoorBrush();
  }
}

void BrushController::activateLockedDoorBrush() {
  if (lockedDoorBrush_) {
    setBrush(lockedDoorBrush_.get());
  }
}

void BrushController::activateQuestDoorBrush() {
  if (questDoorBrush_) {
    setBrush(questDoorBrush_.get());
  }
}

void BrushController::activateMagicDoorBrush() {
  if (magicDoorBrush_) {
    setBrush(magicDoorBrush_.get());
  }
}

void BrushController::activateArchwayBrush() {
  if (archwayBrush_) {
    setBrush(archwayBrush_.get());
  }
}

void BrushController::activateWindowBrush() {
  if (windowBrush_) {
    setBrush(windowBrush_.get());
  }
}

void BrushController::activateHatchWindowBrush() {
  if (hatchWindowBrush_) {
    setBrush(hatchWindowBrush_.get());
  } else {
    activateWindowBrush();
  }
}

bool BrushController::canSwitchDoorAt(const Domain::Position &pos,
                                      const Domain::Item *preferredItem) const {
  if (!map_ || !registry_) {
    return false;
  }

  auto *tile = map_->getTile(pos);
  const auto resolved = resolveDoorTarget(tile, registry_, preferredItem);
  return resolved.wallBrush != nullptr && resolved.item != nullptr;
}

bool BrushController::switchDoorAt(const Domain::Position &pos,
                                   const Domain::Item *preferredItem) {
  if (!map_ || !historyManager_ || !registry_) {
    return false;
  }

  auto *tile = map_->getTile(pos);
  const auto resolved = resolveDoorTarget(tile, registry_, preferredItem);
  if (!tile || !resolved.wallBrush || !resolved.item) {
    return false;
  }

  historyManager_->beginOperation("Switch door",
                                  Domain::History::ActionType::Other, nullptr);
  historyManager_->recordTileBefore(pos, tile);

  const bool preferLocked =
      brushSettingsService_ && brushSettingsService_->getLockDoors();
  if (!resolved.wallBrush->switchDoor(*map_, *tile, resolved.item,
                                      preferLocked)) {
    historyManager_->cancelOperation();
    return false;
  }

  historyManager_->endOperation(map_, nullptr);
  return true;
}

std::optional<bool>
BrushController::getDoorOpenStateAt(const Domain::Position &pos,
                                    const Domain::Item *preferredItem) const {
  if (!map_ || !registry_) {
    return std::nullopt;
  }

  auto *tile = const_cast<Domain::Tile *>(map_->getTile(pos));
  const auto resolved = resolveDoorTarget(tile, registry_, preferredItem);
  if (!resolved.wallBrush || !resolved.item) {
    return std::nullopt;
  }

  if (const auto door = resolved.wallBrush->findDoorForItem(resolved.item->getServerId())) {
    return door->isOpen;
  }

  return std::nullopt;
}

std::optional<uint32_t> BrushController::getCurrentItemId() const {
  // Check if current brush is a RawBrush
  if (auto *rawBrush = dynamic_cast<RawBrush *>(currentBrush_)) {
    return rawBrush->getItemId();
  }
  return std::nullopt;
}

bool BrushController::selectBrushFromTile(const Domain::Tile &tile,
                                          BrushPickMode mode,
                                          const Domain::Item *preferredItem) {
  const auto selection = resolveBrushFromTile(tile, mode, preferredItem);
  return selection.has_value() && applyResolvedSelection(*selection);
}

std::optional<ResolvedBrushSelection>
BrushController::resolveBrushFromTile(const Domain::Tile &tile,
                                      BrushPickMode mode,
                                      const Domain::Item *preferredItem) {
  if (!map_) {
    return std::nullopt;
  }

  auto makeSelection = [](IBrush *brush, BrushPickMode selectionMode,
                          std::string displayName = {}) {
    if (!brush) {
      return std::optional<ResolvedBrushSelection>{};
    }

    if (displayName.empty()) {
      displayName = brush->getName();
    }

    return std::optional<ResolvedBrushSelection>{
        ResolvedBrushSelection{.brush = brush,
                               .mode = selectionMode,
                               .displayName = std::move(displayName)}};
  };

  auto selectFlagBrush = [this, &makeSelection](Domain::TileFlag flag)
      -> std::optional<ResolvedBrushSelection> {
    switch (flag) {
    case Domain::TileFlag::ProtectionZone:
      return makeSelection(&pzBrush_, BrushPickMode::ProtectionZone);
    case Domain::TileFlag::NoPvp:
      return makeSelection(&noPvpBrush_, BrushPickMode::NoPvp);
    case Domain::TileFlag::NoLogout:
      return makeSelection(&noLogoutBrush_, BrushPickMode::NoLogout);
    case Domain::TileFlag::PvpZone:
      return makeSelection(&pvpZoneBrush_, BrushPickMode::PvpZone);
    case Domain::TileFlag::Refresh:
    case Domain::TileFlag::None:
      return std::nullopt;
    }
    return std::nullopt;
  };

  auto selectNamedBrush =
      [&makeSelection](IBrush *brush,
                       BrushPickMode selectionMode) -> std::optional<ResolvedBrushSelection> {
    return makeSelection(brush, selectionMode);
  };

  auto selectBrushById =
      [this, &selectNamedBrush](BrushId brushId,
                                BrushPickMode selectionMode)
      -> std::optional<ResolvedBrushSelection> {
    if (!registry_ || brushId == InvalidBrushId) {
      return std::nullopt;
    }
    return selectNamedBrush(registry_->getBrushById(brushId), selectionMode);
  };

  auto selectPreferredBrushByType =
      [this, preferredItem, &selectBrushById, &selectNamedBrush](
          BrushType type, BrushPickMode selectionMode)
      -> std::optional<ResolvedBrushSelection> {
    if (!preferredItem || !registry_) {
      return std::nullopt;
    }

    if (auto selection =
            selectBrushById(preferredItem->getOwnerBrushId(), selectionMode);
        selection && matchesBrushType(selection->brush, type)) {
      return selection;
    }

    for (auto *brush : registry_->getBrushesForItem(preferredItem->getServerId())) {
      if (matchesBrushType(brush, type)) {
        return selectNamedBrush(brush, selectionMode);
      }
    }

    return std::nullopt;
  };

  auto selectDoorBrush = [this, &tile, preferredItem, &makeSelection,
                          &selectBrushById]()
      -> std::optional<ResolvedBrushSelection> {
    if (preferredItem) {
      if (auto *doorBrush =
              getDoorBrushForType(getDoorTypeForItem(*preferredItem, registry_))) {
        return makeSelection(doorBrush, BrushPickMode::Door);
      }
    }

    for (size_t index = tile.getItemCount(); index > 0; --index) {
      const auto *item = tile.getItem(index - 1);
      if (item) {
        if (auto selection =
                selectBrushById(item->getOwnerBrushId(), BrushPickMode::Door);
            selection && selection->brush &&
            selection->brush->getType() == BrushType::Door) {
          return selection;
        }
      }
    }

    if (auto *doorBrush = getDoorBrushForType(getDoorTypeForTile(tile, registry_))) {
      return makeSelection(doorBrush, BrushPickMode::Door);
    }

    return std::nullopt;
  };

  auto selectGroundBrush = [this, &tile, &selectNamedBrush,
                            &selectBrushById]()
      -> std::optional<ResolvedBrushSelection> {
    if (!registry_ || !tile.hasGround()) {
      return std::nullopt;
    }

    if (auto selection =
            selectBrushById(tile.getGroundBrushId(), BrushPickMode::Ground)) {
      return selection;
    }

    for (auto *brush : registry_->getBrushesForItem(tile.getGround()->getServerId())) {
      if (brush && brush->getType() == BrushType::Ground) {
        return selectNamedBrush(brush, BrushPickMode::Ground);
      }
    }

    return std::nullopt;
  };

  auto selectRawBrush = [this, &tile, preferredItem]()
      -> std::optional<ResolvedBrushSelection> {
    if (preferredItem) {
      return ResolvedBrushSelection{
          .mode = BrushPickMode::Raw,
          .displayName = "RAW Item",
          .rawItemId = preferredItem->getServerId(),
      };
    }

    if (const auto *item = getTopTileItem(tile)) {
      return ResolvedBrushSelection{
          .mode = BrushPickMode::Raw,
          .displayName = "RAW Item",
          .rawItemId = item->getServerId(),
      };
    }
    if (tile.hasGround()) {
      return ResolvedBrushSelection{
          .mode = BrushPickMode::Raw,
          .displayName = "RAW Item",
          .rawItemId = tile.getGround()->getServerId(),
      };
    }
    return std::nullopt;
  };

  auto selectHouseExitBrush = [this, &tile, &makeSelection,
                               &selectBrushById]()
      -> std::optional<ResolvedBrushSelection> {
    auto resolveHouseId = [this, &tile]() -> uint32_t {
      if (tile.getHouseExitHouseId() != 0) {
        return tile.getHouseExitHouseId();
      }

      if (tile.getHouseId() != 0) {
        return tile.getHouseId();
      }

      if (!map_) {
        return 0;
      }

      for (const auto &[houseId, house] : map_->getHouses()) {
        if (house && house->entry_position == tile.getPosition()) {
          return houseId;
        }
      }

      return 0;
    };

    if (tile.getHouseExitBrushId() != InvalidBrushId) {
      auto selection =
          selectBrushById(tile.getHouseExitBrushId(), BrushPickMode::HouseExit);
      if (selection) {
        selection->houseExitHouseId = resolveHouseId();
      }
      return selection;
    }

    if (const auto houseId = resolveHouseId(); houseId != 0) {
      auto selection = makeSelection(&houseExitBrush_, BrushPickMode::HouseExit);
      if (selection) {
        selection->houseExitHouseId = houseId;
      }
      return selection;
    }

    return std::nullopt;
  };

  auto selectWaypointBrush = [this, &tile, &makeSelection,
                              &selectBrushById]()
      -> std::optional<ResolvedBrushSelection> {
    if (tile.getWaypointBrushId() != InvalidBrushId) {
      auto selection =
          selectBrushById(tile.getWaypointBrushId(), BrushPickMode::Waypoint);
      if (selection) {
        if (const auto *waypoint = map_->getWaypointAt(tile.getPosition())) {
          selection->waypointName = waypoint->name;
        }
      }
      return selection;
    }
    if (const auto *waypoint = map_->getWaypointAt(tile.getPosition())) {
      auto selection = makeSelection(&waypointBrush_, BrushPickMode::Waypoint);
      if (selection) {
        selection->waypointName = waypoint->name;
      }
      return selection;
    }
    return std::nullopt;
  };

  auto selectOptionalBorderBrush = [this, &tile, &makeSelection,
                                    &selectBrushById]()
      -> std::optional<ResolvedBrushSelection> {
    if (auto selection = selectBrushById(tile.getOptionalBorderBrushId(),
                                         BrushPickMode::OptionalBorder)) {
      return selection;
    }

    if (!tile.hasOptionalBorder() || !tile.hasGround() || !registry_) {
      return std::nullopt;
    }

    for (auto *brush : registry_->getBrushesForItem(tile.getGround()->getServerId())) {
      auto *groundBrush = dynamic_cast<GroundBrush *>(brush);
      if (groundBrush && groundBrush->hasOptionalBorderRule()) {
        return makeSelection(&optionalBorderBrush_,
                             BrushPickMode::OptionalBorder);
      }
    }
    return std::nullopt;
  };

  auto selectCollectionBrush = [this, &tile, &makeSelection,
                                &selectGroundBrush, &selectPreferredBrushByType]()
      -> std::optional<ResolvedBrushSelection> {
    if (auto selection = selectPreferredBrushByType(BrushType::Wall,
                                                    BrushPickMode::Collection);
        selection && selection->brush && selection->brush->visibleInPalette() &&
        selection->brush->hasCollection()) {
      return selection;
    }
    if (auto selection = selectPreferredBrushByType(BrushType::Table,
                                                    BrushPickMode::Collection);
        selection && selection->brush && selection->brush->visibleInPalette() &&
        selection->brush->hasCollection()) {
      return selection;
    }
    if (auto selection = selectPreferredBrushByType(BrushType::Carpet,
                                                    BrushPickMode::Collection);
        selection && selection->brush && selection->brush->visibleInPalette() &&
        selection->brush->hasCollection()) {
      return selection;
    }
    if (auto selection = selectPreferredBrushByType(BrushType::Doodad,
                                                    BrushPickMode::Collection);
        selection && selection->brush && selection->brush->visibleInPalette() &&
        selection->brush->hasCollection()) {
      return selection;
    }
    if (auto selection = selectPreferredBrushByType(BrushType::Raw,
                                                    BrushPickMode::Collection);
        selection && selection->brush && selection->brush->visibleInPalette() &&
        selection->brush->hasCollection()) {
      return selection;
    }

    const auto selectCollectionType =
        [&](BrushType type) -> std::optional<ResolvedBrushSelection> {
      if (auto *brush =
              findOwnedItemBrushByType(tile, registry_, type, true)) {
        return makeSelection(brush, BrushPickMode::Collection);
      }
      if (auto *brush = findItemBrushByType(tile, registry_, type, true)) {
        return makeSelection(brush, BrushPickMode::Collection);
      }
      return std::nullopt;
    };

    for (const auto type : {BrushType::Wall, BrushType::Table, BrushType::Carpet,
                            BrushType::Doodad, BrushType::Raw}) {
      if (auto selection = selectCollectionType(type)) {
        return selection;
      }
    }

    if (auto selection = selectGroundBrush()) {
      if (selection->brush && selection->brush->visibleInPalette() &&
          selection->brush->hasCollection()) {
        selection->mode = BrushPickMode::Collection;
        return selection;
      }
    }

    return std::nullopt;
  };

  auto chooseFirst = [](std::optional<ResolvedBrushSelection> first,
                        std::optional<ResolvedBrushSelection> second)
      -> std::optional<ResolvedBrushSelection> {
    return first ? std::move(first) : std::move(second);
  };

  auto selectSpecificMode = [this, &tile, &selectGroundBrush, &selectNamedBrush,
                             &selectDoorBrush, &selectRawBrush,
                             &selectWaypointBrush, &selectHouseExitBrush,
                             &selectOptionalBorderBrush, &selectFlagBrush,
                             &selectCollectionBrush, &selectBrushById,
                             &selectPreferredBrushByType, &makeSelection,
                             &chooseFirst](BrushPickMode pickMode)
      -> std::optional<ResolvedBrushSelection> {
    switch (pickMode) {
    case BrushPickMode::Smart:
      return std::nullopt;
    case BrushPickMode::Raw:
      return selectRawBrush();
    case BrushPickMode::Ground:
      return selectGroundBrush();
    case BrushPickMode::Doodad:
      return chooseFirst(
          selectPreferredBrushByType(BrushType::Doodad,
                                     BrushPickMode::Doodad),
          chooseFirst(
          selectNamedBrush(
              findOwnedItemBrushByType(tile, registry_, BrushType::Doodad),
              BrushPickMode::Doodad),
          selectNamedBrush(
              findItemBrushByType(tile, registry_, BrushType::Doodad),
              BrushPickMode::Doodad)));
    case BrushPickMode::Collection:
      return selectCollectionBrush();
    case BrushPickMode::Door:
      return selectDoorBrush();
    case BrushPickMode::Wall:
      return chooseFirst(
          selectPreferredBrushByType(BrushType::Wall, BrushPickMode::Wall),
          chooseFirst(
          selectNamedBrush(
              findOwnedItemBrushByType(tile, registry_, BrushType::Wall),
              BrushPickMode::Wall),
          selectNamedBrush(
              findItemBrushByType(tile, registry_, BrushType::Wall),
              BrushPickMode::Wall)));
    case BrushPickMode::Carpet:
      return chooseFirst(
          selectPreferredBrushByType(BrushType::Carpet,
                                     BrushPickMode::Carpet),
          chooseFirst(
          selectNamedBrush(
              findOwnedItemBrushByType(tile, registry_, BrushType::Carpet),
              BrushPickMode::Carpet),
          selectNamedBrush(
              findItemBrushByType(tile, registry_, BrushType::Carpet),
              BrushPickMode::Carpet)));
    case BrushPickMode::Table:
      return chooseFirst(
          selectPreferredBrushByType(BrushType::Table, BrushPickMode::Table),
          chooseFirst(
          selectNamedBrush(
              findOwnedItemBrushByType(tile, registry_, BrushType::Table),
              BrushPickMode::Table),
          selectNamedBrush(
              findItemBrushByType(tile, registry_, BrushType::Table),
              BrushPickMode::Table)));
    case BrushPickMode::Creature:
      if (tile.getCreatureBrushId() != InvalidBrushId) {
        return selectBrushById(tile.getCreatureBrushId(),
                               BrushPickMode::Creature);
      }
      if (!registry_ || !tile.hasCreature()) {
        return std::nullopt;
      }
      return selectNamedBrush(
          registry_->getBrushForCreature(tile.getCreature()->name),
          BrushPickMode::Creature);
    case BrushPickMode::Spawn:
      if (tile.getSpawnBrushId() != InvalidBrushId) {
        return selectBrushById(tile.getSpawnBrushId(), BrushPickMode::Spawn);
      }
      if (!tile.hasSpawn()) {
        return std::nullopt;
      }
      return makeSelection(&spawnBrush_, BrushPickMode::Spawn);
    case BrushPickMode::House:
      if (tile.getHouseBrushId() != InvalidBrushId) {
        auto selection =
            selectBrushById(tile.getHouseBrushId(), BrushPickMode::House);
        if (selection) {
          selection->houseId = tile.getHouseId();
        }
        return selection;
      }
      if (!tile.isHouseTile()) {
        return std::nullopt;
      }
      if (auto selection = makeSelection(&houseBrush_, BrushPickMode::House)) {
        selection->houseId = tile.getHouseId();
        return selection;
      }
      return std::nullopt;
    case BrushPickMode::HouseExit:
      return selectHouseExitBrush();
    case BrushPickMode::Waypoint:
      return selectWaypointBrush();
    case BrushPickMode::OptionalBorder:
      return selectOptionalBorderBrush();
    case BrushPickMode::ProtectionZone:
      return tile.hasFlag(Domain::TileFlag::ProtectionZone)
                 ? chooseFirst(
                       selectBrushById(
                           tile.getZoneBrushId(
                               Domain::TileFlag::ProtectionZone),
                           BrushPickMode::ProtectionZone),
                       selectFlagBrush(Domain::TileFlag::ProtectionZone))
                 : std::nullopt;
    case BrushPickMode::NoPvp:
      return tile.hasFlag(Domain::TileFlag::NoPvp)
                 ? chooseFirst(
                       selectBrushById(
                           tile.getZoneBrushId(Domain::TileFlag::NoPvp),
                           BrushPickMode::NoPvp),
                       selectFlagBrush(Domain::TileFlag::NoPvp))
                 : std::nullopt;
    case BrushPickMode::NoLogout:
      return tile.hasFlag(Domain::TileFlag::NoLogout)
                 ? chooseFirst(
                       selectBrushById(
                           tile.getZoneBrushId(Domain::TileFlag::NoLogout),
                           BrushPickMode::NoLogout),
                       selectFlagBrush(Domain::TileFlag::NoLogout))
                 : std::nullopt;
    case BrushPickMode::PvpZone:
      return tile.hasFlag(Domain::TileFlag::PvpZone)
                 ? chooseFirst(
                       selectBrushById(
                           tile.getZoneBrushId(Domain::TileFlag::PvpZone),
                           BrushPickMode::PvpZone),
                       selectFlagBrush(Domain::TileFlag::PvpZone))
                 : std::nullopt;
    }

    return std::nullopt;
  };

  if (mode != BrushPickMode::Smart) {
    return selectSpecificMode(mode);
  }

  if (auto selection =
          selectBrushById(tile.getCreatureBrushId(), BrushPickMode::Creature)) {
    return selection;
  }

  if (tile.getSpawnBrushId() != InvalidBrushId) {
    if (auto selection =
            selectBrushById(tile.getSpawnBrushId(), BrushPickMode::Spawn)) {
      return selection;
    }
  }

  if (registry_ && tile.hasCreature()) {
    if (auto *brush = registry_->getBrushForCreature(tile.getCreature()->name)) {
      return makeSelection(brush, BrushPickMode::Creature);
    }
  }

  if (tile.hasSpawn()) {
    return makeSelection(&spawnBrush_, BrushPickMode::Spawn);
  }

  if (auto selection = selectWaypointBrush()) {
    return selection;
  }

  if (auto selection = selectHouseExitBrush()) {
    return selection;
  }

  if (tile.isHouseTile()) {
    if (auto selection =
            selectBrushById(tile.getHouseBrushId(), BrushPickMode::House)) {
      selection->houseId = tile.getHouseId();
      return selection;
    }
    auto selection = makeSelection(&houseBrush_, BrushPickMode::House);
    if (selection) {
      selection->houseId = tile.getHouseId();
    }
    return selection;
  }

  if (auto selection = selectDoorBrush()) {
    return selection;
  }

  if (preferredItem) {
    for (const auto candidate : {BrushType::Doodad, BrushType::Wall,
                                 BrushType::Table, BrushType::Carpet}) {
      if (auto selection =
              selectPreferredBrushByType(candidate, BrushPickMode::Smart)) {
        return selection;
      }
    }
  }

  for (const auto candidate : {BrushType::Doodad, BrushType::Wall,
                               BrushType::Table, BrushType::Carpet}) {
    if (auto *brush = findOwnedItemBrushByType(tile, registry_, candidate)) {
      return makeSelection(brush, BrushPickMode::Smart);
    }
    if (auto *brush = findItemBrushByType(tile, registry_, candidate)) {
      return makeSelection(brush, BrushPickMode::Smart);
    }
  }

  if (registry_) {
    if (auto *brush = registry_->resolveBrushForTile(tile)) {
      auto selection = makeSelection(brush, BrushPickMode::Smart);
      if (!selection) {
        return std::nullopt;
      }

      switch (brush->getType()) {
      case BrushType::House:
        selection->houseId = tile.getHouseId();
        break;
      case BrushType::HouseExit:
        if (tile.getHouseExitHouseId() != 0) {
          selection->houseExitHouseId = tile.getHouseExitHouseId();
        } else if (tile.getHouseId() != 0) {
          selection->houseExitHouseId = tile.getHouseId();
        }
        break;
      case BrushType::Waypoint:
        if (const auto *waypoint = map_->getWaypointAt(tile.getPosition())) {
          selection->waypointName = waypoint->name;
        }
        break;
      default:
        break;
      }

      return selection;
    }
  }

  if (auto selection = selectOptionalBorderBrush()) {
    return selection;
  }

  for (const auto flag : {Domain::TileFlag::ProtectionZone,
                          Domain::TileFlag::NoPvp,
                          Domain::TileFlag::NoLogout,
                          Domain::TileFlag::PvpZone}) {
    if (tile.hasFlag(flag)) {
      if (auto selection = selectFlagBrush(flag)) {
        return selection;
      }
    }
  }

  return std::nullopt;
}

bool BrushController::applyResolvedSelection(
    const ResolvedBrushSelection &selection) {
  if (selection.rawItemId.has_value()) {
    if (!registry_) {
      return false;
    }

    if (auto *rawBrush = registry_->getOrCreateRAWBrush(*selection.rawItemId)) {
      variation_ = 0;
      rawBrush->setVariation(0);
      setBrush(rawBrush);
      return true;
    }
  }

  if (!selection.brush) {
    return false;
  }

  if (selection.brush == &houseBrush_ && selection.houseId.has_value()) {
    houseBrush_.setHouseId(*selection.houseId);
  }

  if (selection.brush == &houseExitBrush_ &&
      selection.houseExitHouseId.has_value()) {
    houseExitBrush_.setHouseId(*selection.houseExitHouseId);
  }

  if (selection.brush == &waypointBrush_ && selection.waypointName.has_value()) {
    waypointBrush_.setWaypointName(*selection.waypointName);
  }

  variation_ = std::max(0, selection.variation);
  if (selection.brush) {
    selection.brush->setVariation(static_cast<size_t>(variation_));
  }

  setBrush(selection.brush);
  return true;
}

DoorBrush *BrushController::getDoorBrushForType(DoorType type) const {
  switch (type) {
    case DoorType::Normal:
      return normalDoorBrush_.get();
    case DoorType::NormalAlt:
      return normalAltDoorBrush_ ? normalAltDoorBrush_.get()
                                 : normalDoorBrush_.get();
    case DoorType::Locked:
      return lockedDoorBrush_.get();
    case DoorType::Quest:
      return questDoorBrush_.get();
    case DoorType::Magic:
      return magicDoorBrush_.get();
    case DoorType::Archway:
      return archwayBrush_.get();
    case DoorType::Window:
      return windowBrush_.get();
    case DoorType::HatchWindow:
      return hatchWindowBrush_ ? hatchWindowBrush_.get() : windowBrush_.get();
    case DoorType::Undefined:
      return nullptr;
    }
  return nullptr;
}

bool BrushController::applyBrush(const Domain::Position &pos) {
  return applyBrush(pos, 0);
}

bool BrushController::applyBrush(const Domain::Position &pos,
                                uint32_t modifiers) {
  if (!map_ || !historyManager_ || !currentBrush_) {
    return false;
  }

  strokeModifiers_ = modifiers;

  // If in stroke mode, use optimized direct painting
  if (strokeActive_) {
    auto key = std::make_tuple(pos.x, pos.y, pos.z);
    if (paintedPositions_.count(key) > 0) {
      return true; // Already painted
    }
    paintedPositions_.insert(key);

    // Capture BEFORE state for undo
    const Domain::Tile *tile = map_->getTile(pos);
    historyManager_->recordTileBefore(pos, tile);

    if (strokeEraseMode_) {
      if (auto *mutableTile = map_->getTile(pos)) {
        currentBrush_->undraw(*map_, mutableTile);
      }
    } else {
      paintTileDirect(pos, modifiers);
    }
    return true;
  }

  // Single click mode - use HistoryManager for undo support
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation("Brush: " + currentBrushName_,
                                  Domain::History::ActionType::Draw, nullptr);
  paintedPositions_.clear();
  lastStrokePos_.reset();

  switch (getActionFamily()) {
  case BrushActionFamily::GroundLike:
    paintExpandedCenter(pos, modifiers);
    break;
  case BrushActionFamily::WallLike:
    if (currentBrush_->getType() == BrushType::Wall &&
        (modifiers & GLFW_MOD_ALT) != 0 &&
        getBrushPositionsForCenter(pos).size() == 1) {
      paintRecordedPosition(pos, modifiers, true);
    } else {
      paintExpandedCenter(pos, modifiers);
    }
    break;
  case BrushActionFamily::DoorLike:
  case BrushActionFamily::PointLike:
    paintRecordedPosition(pos, modifiers);
    break;
  case BrushActionFamily::DoodadLike:
    paintDoodadRecordedPosition(pos, modifiers);
    break;
  }

  historyManager_->endOperation(map_, nullptr);
  paintedPositions_.clear();
  lastStrokePos_.reset();

  return true;
}

bool BrushController::eraseBrush(const Domain::Position &pos) {
  return eraseBrush(pos, 0);
}

bool BrushController::eraseBrush(const Domain::Position &pos,
                                 uint32_t modifiers) {
  if (!map_ || !historyManager_ || !currentBrush_) {
    return false;
  }

  Domain::Tile *tile = map_->getTile(pos);
  if (!tile) {
    return false;
  }

  // Use HistoryManager for undo support
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation("Erase: " + currentBrushName_,
                                  Domain::History::ActionType::Delete, nullptr);
  historyManager_->recordTileBefore(pos, tile);
  strokeModifiers_ = modifiers;

  // Use IBrush::undraw() for unified erasing
  currentBrush_->undraw(*map_, tile);

  historyManager_->endOperation(map_, nullptr);

  return true;
}

void BrushController::beginStroke() {
  beginStroke(0);
}

void BrushController::beginStroke(uint32_t modifiers) {
  beginStroke(modifiers, false);
}

void BrushController::beginStroke(uint32_t modifiers, bool eraseMode) {
  if (!historyManager_ || !currentBrush_)
    return;

  if (currentBrush_->getType() == BrushType::Ground) {
    GroundBrush::resetAltReplaceState();
  }

  // Start a new history operation for this stroke
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation(
      (eraseMode ? "Erase: " : "Brush: ") + currentBrushName_,
      eraseMode ? Domain::History::ActionType::Delete
                : Domain::History::ActionType::Draw,
      nullptr);

  // Set stroke active flag (HistoryManager handles actual undo)
  strokeActive_ = true;
  strokeEraseMode_ = eraseMode;
  strokeModifiers_ = modifiers;
  paintedPositions_.clear();
  lastStrokePos_.reset();
  spdlog::debug("[BrushController] Started {} stroke",
                eraseMode ? "erase" : "draw");
}

// Paint tile using IBrush::draw()
void BrushController::paintTileDirect(const Domain::Position &pos,
                                      uint32_t modifiers,
                                      bool specialAction) {
  if (!map_ || !currentBrush_)
    return;

  if (!currentBrush_->canDraw(*map_, pos)) {
    return;
  }

  Domain::Tile *tile = map_->getOrCreateTile(pos);
  if (!tile)
    return;

  // Create draw context with brush settings
  DrawContext ctx;
  ctx.variation = variation_;
  ctx.modifiers = modifiers;
  ctx.isDragging = strokeActive_;
  ctx.specialAction = specialAction;
  ctx.forcePlace = (modifiers & GLFW_MOD_ALT) != 0;
  ctx.brushSettings = brushSettingsService_;
  ctx.clientData = clientData_;
  ctx.brushRegistry = registry_;
  ctx.ownerBrushId =
      registry_ ? registry_->getBrushId(currentBrush_) : InvalidBrushId;

  // Use unified IBrush::draw()
  currentBrush_->draw(*map_, tile, ctx);
}

ResolvedBrushSelection BrushController::captureCurrentSelection() const {
  ResolvedBrushSelection selection;
  selection.brush = currentBrush_;
  selection.displayName = currentBrush_ ? currentBrush_->getName() : std::string{};
  selection.variation = variation_;

  if (!currentBrush_) {
    return selection;
  }

  if (const auto *rawBrush = dynamic_cast<const RawBrush *>(currentBrush_)) {
    selection.rawItemId = rawBrush->getItemId();
  }

  if (currentBrush_ == &houseBrush_) {
    selection.houseId = houseBrush_.getHouseId();
  }

  if (currentBrush_ == &houseExitBrush_) {
    selection.houseExitHouseId = houseExitBrush_.getHouseId();
  }

  if (currentBrush_ == &waypointBrush_) {
    selection.waypointName = waypointBrush_.getWaypointName();
  }

  return selection;
}

void BrushController::continueStroke(const Domain::Position &pos) {
  if (!strokeActive_ || !historyManager_ || !currentBrush_)
    return;

  switch (getActionFamily()) {
  case BrushActionFamily::GroundLike:
    continueGroundLikeStroke(pos);
    break;
  case BrushActionFamily::WallLike:
    continueWallLikeStroke(pos);
    break;
  case BrushActionFamily::DoorLike:
    continueDoorLikeStroke(pos);
    break;
  case BrushActionFamily::DoodadLike:
    continueDoodadLikeStroke(pos);
    break;
  case BrushActionFamily::PointLike:
    continuePointLikeStroke(pos);
    break;
  }
}

BrushController::BrushActionFamily BrushController::getActionFamily() const {
  if (!currentBrush_) {
    return BrushActionFamily::PointLike;
  }

  switch (currentBrush_->getType()) {
  case BrushType::Ground:
  case BrushType::OptionalBorder:
  case BrushType::Flag:
  case BrushType::Eraser:
    return BrushActionFamily::GroundLike;
  case BrushType::Wall:
  case BrushType::WallDecoration:
  case BrushType::Table:
  case BrushType::Carpet:
    return BrushActionFamily::WallLike;
  case BrushType::Door:
    return BrushActionFamily::DoorLike;
  case BrushType::Doodad:
    return BrushActionFamily::DoodadLike;
  case BrushType::Raw:
  case BrushType::Creature:
  case BrushType::Spawn:
  case BrushType::House:
  case BrushType::HouseExit:
  case BrushType::Waypoint:
  case BrushType::Placeholder:
    return BrushActionFamily::PointLike;
  }

  return BrushActionFamily::PointLike;
}

std::vector<Domain::Position>
BrushController::getBrushPositionsForCenter(const Domain::Position &center) const {
  if (!currentBrush_ || !brushSettingsService_) {
    return {center};
  }

  switch (getActionFamily()) {
  case BrushActionFamily::DoorLike:
  case BrushActionFamily::PointLike:
  case BrushActionFamily::DoodadLike:
    return {center};
  case BrushActionFamily::GroundLike:
  case BrushActionFamily::WallLike:
    break;
  }

  return brushSettingsService_->getBrushPositions(center);
}

void BrushController::paintRecordedPosition(const Domain::Position &pos,
                                            uint32_t modifiers,
                                            bool specialAction) {
  if (!map_ || !historyManager_) {
    return;
  }

  auto key = std::make_tuple(pos.x, pos.y, pos.z);
  if (paintedPositions_.contains(key)) {
    return;
  }

  paintedPositions_.insert(key);
  historyManager_->recordTileBefore(pos, map_->getTile(pos));
  paintTileDirect(pos, modifiers, specialAction);
}

void BrushController::eraseRecordedPosition(const Domain::Position &pos) {
  if (!map_ || !historyManager_ || !currentBrush_) {
    return;
  }

  auto key = std::make_tuple(pos.x, pos.y, pos.z);
  if (paintedPositions_.contains(key)) {
    return;
  }

  auto *tile = map_->getTile(pos);
  if (!tile) {
    return;
  }

  paintedPositions_.insert(key);
  historyManager_->recordTileBefore(pos, tile);
  currentBrush_->undraw(*map_, tile);
}

void BrushController::paintDoodadRecordedPosition(const Domain::Position &pos,
                                                  uint32_t modifiers) {
  if (!map_ || !historyManager_) {
    return;
  }

  auto *doodadBrush = dynamic_cast<DoodadBrush *>(currentBrush_);
  if (!doodadBrush) {
    paintRecordedPosition(pos, modifiers);
    return;
  }

  const auto forcePlace = (modifiers & GLFW_MOD_ALT) != 0;
  const auto layout = doodadBrush->buildPlacementLayout(
      pos, brushSettingsService_, static_cast<size_t>(variation_), map_,
      forcePlace);
  if (layout.empty()) {
    return;
  }

  for (const auto &layoutTile : layout) {
    const Domain::Position absolutePosition(
        pos.x + layoutTile.relativePosition.x, pos.y + layoutTile.relativePosition.y,
        static_cast<int16_t>(pos.z + layoutTile.relativePosition.z));
    auto key = std::make_tuple(absolutePosition.x, absolutePosition.y,
                               absolutePosition.z);
    if (paintedPositions_.contains(key)) {
      continue;
    }

    paintedPositions_.insert(key);
    historyManager_->recordTileBefore(absolutePosition, map_->getTile(absolutePosition));
  }

  DrawContext ctx;
  ctx.variation = variation_;
  ctx.modifiers = modifiers;
  ctx.isDragging = strokeActive_;
  ctx.forcePlace = forcePlace;
  ctx.brushSettings = brushSettingsService_;
  ctx.clientData = clientData_;
  ctx.brushRegistry = registry_;
  ctx.ownerBrushId =
      registry_ ? registry_->getBrushId(currentBrush_) : InvalidBrushId;
  doodadBrush->applyPlacementLayout(*map_, pos, layout, ctx);
}

void BrushController::eraseDoodadRecordedPosition(const Domain::Position &pos,
                                                  uint32_t modifiers) {
  if (!map_ || !historyManager_) {
    return;
  }

  auto *doodadBrush = dynamic_cast<DoodadBrush *>(currentBrush_);
  if (!doodadBrush) {
    eraseRecordedPosition(pos);
    return;
  }

  const auto forcePlace = (modifiers & GLFW_MOD_ALT) != 0;
  const auto positions = doodadBrush->getPlacementPositions(
      pos, brushSettingsService_, static_cast<size_t>(variation_), map_,
      forcePlace);
  if (positions.empty()) {
    eraseRecordedPosition(pos);
    return;
  }

  for (const auto &absolutePosition : positions) {
    auto key = std::make_tuple(absolutePosition.x, absolutePosition.y,
                               absolutePosition.z);
    if (paintedPositions_.contains(key)) {
      continue;
    }

    auto *tile = map_->getTile(absolutePosition);
    if (!tile) {
      continue;
    }

    paintedPositions_.insert(key);
    historyManager_->recordTileBefore(absolutePosition, tile);
    doodadBrush->undraw(*map_, tile);
  }
}

void BrushController::paintExpandedCenter(const Domain::Position &center,
                                          uint32_t modifiers) {
  for (const auto &pos : getBrushPositionsForCenter(center)) {
    paintRecordedPosition(pos, modifiers);
  }
}

void BrushController::eraseExpandedCenter(const Domain::Position &center) {
  for (const auto &pos : getBrushPositionsForCenter(center)) {
    eraseRecordedPosition(pos);
  }
}

void BrushController::continueGroundLikeStroke(const Domain::Position &pos) {
  if (!lastStrokePos_.has_value()) {
    if (strokeEraseMode_) {
      eraseExpandedCenter(pos);
    } else {
      paintExpandedCenter(pos, strokeModifiers_);
    }
    lastStrokePos_ = pos;
    return;
  }

  for (const auto &linePos : getLinePositions(lastStrokePos_.value(), pos)) {
    if (strokeEraseMode_) {
      eraseExpandedCenter(linePos);
    } else {
      paintExpandedCenter(linePos, strokeModifiers_);
    }
  }

  lastStrokePos_ = pos;
}

void BrushController::continueWallLikeStroke(const Domain::Position &pos) {
  const bool altPressed = (strokeModifiers_ & GLFW_MOD_ALT) != 0;
  const bool wallVariantShift =
      !strokeEraseMode_ && altPressed && currentBrush_ &&
      currentBrush_->getType() == BrushType::Wall &&
      getBrushPositionsForCenter(pos).size() == 1;

  if (!lastStrokePos_.has_value()) {
    if (wallVariantShift) {
      paintRecordedPosition(pos, strokeModifiers_, true);
    } else if (strokeEraseMode_) {
      eraseExpandedCenter(pos);
    } else if (altPressed && !strokeEraseMode_) {
      paintRecordedPosition(pos, strokeModifiers_);
    } else {
      paintExpandedCenter(pos, strokeModifiers_);
    }
    lastStrokePos_ = pos;
    return;
  }

  for (const auto &linePos : getLinePositions(lastStrokePos_.value(), pos)) {
    if (strokeEraseMode_) {
      eraseExpandedCenter(linePos);
    } else if (altPressed) {
      paintRecordedPosition(linePos, strokeModifiers_);
    } else {
      paintExpandedCenter(linePos, strokeModifiers_);
    }
  }

  lastStrokePos_ = pos;
}

void BrushController::continueDoorLikeStroke(const Domain::Position &pos) {
  if (!lastStrokePos_.has_value()) {
    if (strokeEraseMode_) {
      eraseRecordedPosition(pos);
    } else {
      paintRecordedPosition(pos, strokeModifiers_);
    }
    lastStrokePos_ = pos;
    return;
  }

  for (const auto &linePos : getLinePositions(lastStrokePos_.value(), pos)) {
    if (strokeEraseMode_) {
      eraseRecordedPosition(linePos);
    } else {
      paintRecordedPosition(linePos, strokeModifiers_);
    }
  }

  lastStrokePos_ = pos;
}

void BrushController::continueDoodadLikeStroke(const Domain::Position &pos) {
  if (!lastStrokePos_.has_value()) {
    if (strokeEraseMode_) {
      eraseDoodadRecordedPosition(pos, strokeModifiers_);
    } else {
      paintDoodadRecordedPosition(pos, strokeModifiers_);
    }
    lastStrokePos_ = pos;
    return;
  }

  for (const auto &linePos : getLinePositions(lastStrokePos_.value(), pos)) {
    if (strokeEraseMode_) {
      eraseDoodadRecordedPosition(linePos, strokeModifiers_);
    } else {
      paintDoodadRecordedPosition(linePos, strokeModifiers_);
    }
  }

  lastStrokePos_ = pos;
}

void BrushController::continuePointLikeStroke(const Domain::Position &pos) {
  continueDoorLikeStroke(pos);
}

void BrushController::endStroke() {
  if (!strokeActive_ || !historyManager_) {
    if (currentBrush_ && currentBrush_->getType() == BrushType::Ground) {
      GroundBrush::resetAltReplaceState();
    }
    strokeActive_ = false;
    strokeEraseMode_ = false;
    paintedPositions_.clear();
    lastStrokePos_.reset();
    strokeModifiers_ = 0;
    return;
  }

  if (!paintedPositions_.empty()) {
    spdlog::debug("[BrushController] Ended stroke with {} tiles",
                  paintedPositions_.size());

    const bool wallAltStroke = !strokeEraseMode_ && currentBrush_ &&
                               currentBrush_->getType() == BrushType::Wall &&
                               (strokeModifiers_ & GLFW_MOD_ALT) != 0;
    if (wallAltStroke) {
      if (auto *wallBrush = dynamic_cast<WallBrush *>(currentBrush_)) {
        for (const auto &[x, y, z] : paintedPositions_) {
          wallBrush->rebuildAround(*map_, Domain::Position(x, y, z));
        }
      }
    }

    // End the history operation - captures AFTER states and pushes to history
    historyManager_->endOperation(map_, nullptr);
  } else {
    // No tiles painted - cancel the operation
    historyManager_->cancelOperation();
  }

  if (currentBrush_ && currentBrush_->getType() == BrushType::Ground) {
    GroundBrush::resetAltReplaceState();
  }

  strokeActive_ = false;
  strokeEraseMode_ = false;
  paintedPositions_.clear();
  lastStrokePos_.reset();
  strokeModifiers_ = 0;
}

bool BrushController::refreshCurrentBrush() {
  if (!currentBrush_) {
    return false;
  }

  return applyResolvedSelection(captureCurrentSelection());
}

void BrushController::cycleBrushVariation(int delta) {
  if (!currentBrush_ || delta == 0) {
    return;
  }

  const auto maxVariation = static_cast<int>(currentBrush_->getMaxVariation());
  if (maxVariation <= 0) {
    variation_ = 0;
    return;
  }

  variation_ += delta;
  while (variation_ < 0) {
    variation_ += maxVariation;
  }
  while (variation_ >= maxVariation) {
    variation_ -= maxVariation;
  }

  currentBrush_->setVariation(static_cast<size_t>(variation_));
  refreshCurrentBrush();
}

void BrushController::setBrushVariation(int variation) {
  variation_ = std::max(0, variation);
  if (currentBrush_) {
    currentBrush_->setVariation(static_cast<size_t>(variation_));
  }
  refreshCurrentBrush();
}

void BrushController::setBrushThickness(float thickness) {
  if (auto *doodadBrush = dynamic_cast<DoodadBrush *>(currentBrush_)) {
    doodadBrush->setThickness(std::clamp(thickness, 0.0f, 1.0f));
    refreshCurrentBrush();
  }
}

float BrushController::getBrushThickness() const {
  if (const auto *doodadBrush = dynamic_cast<const DoodadBrush *>(currentBrush_)) {
    return doodadBrush->getThickness();
  }
  return 1.0f;
}

void BrushController::adjustBrushSize(int delta) {
  if (delta == 0) {
    return;
  }

  if (brushSettingsService_) {
    brushSettingsService_->setStandardSize(
        brushSettingsService_->getStandardSize() + delta);
    return;
  }

  setBrushSize(brushSize_ + delta);
}

bool BrushController::storeBrushSlot(size_t slot) {
  if (slot >= brushHotkeys_.size() || !currentBrush_) {
    return false;
  }

  brushHotkeys_[slot] = captureCurrentSelection();
  return true;
}

bool BrushController::recallBrushSlot(size_t slot) {
  if (slot >= brushHotkeys_.size() || !brushHotkeys_[slot].has_value()) {
    return false;
  }

  return applyResolvedSelection(*brushHotkeys_[slot]);
}

// Bresenham's line algorithm implementation
std::vector<Domain::Position>
BrushController::getLinePositions(const Domain::Position &from,
                                  const Domain::Position &to) const {

  std::vector<Domain::Position> positions;

  int32_t x0 = from.x, y0 = from.y;
  int32_t x1 = to.x, y1 = to.y;
  int16_t z = from.z; // Stay on same floor

  int32_t dx = std::abs(x1 - x0);
  int32_t dy = -std::abs(y1 - y0);
  int32_t sx = x0 < x1 ? 1 : -1;
  int32_t sy = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;

  while (true) {
    positions.push_back({x0, y0, z});

    if (x0 == x1 && y0 == y1)
      break;

    int32_t e2 = 2 * err;
    if (e2 >= dy) {
      if (x0 == x1)
        break;
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      if (y0 == y1)
        break;
      err += dx;
      y0 += sy;
    }
  }

  return positions;
}

} // namespace MapEditor::Brushes
