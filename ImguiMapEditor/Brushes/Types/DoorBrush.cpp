#include "DoorBrush.h"

#include "Brushes/BrushRegistry.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"
#include "Brushes/Types/WallBrush.h"
#include "Services/BrushSettingsService.h"
#include <GLFW/glfw3.h>

namespace MapEditor::Brushes {

namespace {

bool resolveDoorOpenState(const Domain::Tile &tile, const WallBrush &wallBrush) {
  for (const auto &item : tile.getItems()) {
    if (!item) {
      continue;
    }

    if (const auto door = wallBrush.findDoorForItem(item->getServerId()); door) {
      return door->isOpen;
    }
  }

  return false;
}

} // namespace

DoorBrush::DoorBrush(std::string name, uint32_t lookId, DoorType doorType,
                     BrushRegistry &registry)
    : BrushBase(std::move(name), lookId, true), doorType_(doorType),
      registry_(registry) {}

bool DoorBrush::canDraw(const Domain::ChunkedMap &map,
                        const Domain::Position &pos) const {
  const auto *tile = map.getTile(pos);
  const auto *wallBrush = findWallBrush(tile);
  if (!tile || !wallBrush) {
    return false;
  }

  const bool open = open_ || resolveDoorOpenState(*tile, *wallBrush);
  return wallBrush->canApplyDoor(*tile, doorType_, open, false) ||
         wallBrush->canApplyDoor(*tile, doorType_, open, true);
}

void DoorBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                     const DrawContext &ctx) {
  if (!tile) {
    return;
  }

  if (auto *wallBrush = findWallBrush(tile)) {
    const bool open = ((ctx.modifiers & GLFW_MOD_ALT) != 0) ||
                      open_ || resolveDoorOpenState(*tile, *wallBrush);
    const bool lockedPref =
        ctx.brushSettings && ctx.brushSettings->getLockDoors();
    wallBrush->applyDoor(map, *tile, doorType_, open,
                         doorType_ == DoorType::Locked || lockedPref,
                         ctx.ownerBrushId);
  }
}

void DoorBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }

  if (auto *wallBrush = findWallBrush(tile)) {
    wallBrush->removeDoor(map, *tile);
  }
}

WallBrush *DoorBrush::findWallBrush(const Domain::Tile *tile) const {
  if (!tile) {
    return nullptr;
  }

  auto *fallbackBrush = static_cast<WallBrush *>(nullptr);
  for (const auto &item : tile->getItems()) {
    if (!item) {
      continue;
    }

    if (item->getOwnerBrushId() != InvalidBrushId) {
      if (auto *brush =
              dynamic_cast<WallBrush *>(registry_.getBrushById(item->getOwnerBrushId()))) {
        if (brush->getType() != BrushType::WallDecoration) {
          return brush;
        }

        fallbackBrush = fallbackBrush ? fallbackBrush : brush;
      }
    }

    for (auto *brush : registry_.getBrushesForItem(item->getServerId())) {
      if (auto *wallBrush = dynamic_cast<WallBrush *>(brush)) {
        if (wallBrush->getType() != BrushType::WallDecoration) {
          return wallBrush;
        }

        fallbackBrush = fallbackBrush ? fallbackBrush : wallBrush;
      }
    }
  }

  return fallbackBrush;
}

} // namespace MapEditor::Brushes
