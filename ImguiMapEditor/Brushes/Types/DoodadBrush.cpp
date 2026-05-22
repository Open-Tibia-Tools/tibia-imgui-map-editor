#include "DoodadBrush.h"

#include "BrushUtils.h"
#include "Brushes/Behaviors/WeightedSelection.h"
#include "Brushes/BrushRegistry.h"
#include "Brushes/Types/CarpetBrush.h"
#include "Brushes/Types/GroundBrush.h"
#include "Brushes/Types/TableBrush.h"
#include "Brushes/Types/WallBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Tile.h"
#include "Services/BrushSettingsService.h"
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <unordered_set>

namespace MapEditor::Brushes {

namespace {

uint32_t totalSingleChance(const DoodadAlternative &alternative) {
  return std::accumulate(alternative.getSingleItems().begin(),
                         alternative.getSingleItems().end(), 0u,
                         [](uint32_t sum, const SingleItem &item) {
                           return sum + (item.chance == 0 ? 1u : item.chance);
                         });
}

uint32_t totalCompositeChance(const DoodadAlternative &alternative) {
  return std::accumulate(alternative.getComposites().begin(),
                         alternative.getComposites().end(), 0u,
                         [](uint32_t sum, const CompositeItem &item) {
                           return sum + (item.chance == 0 ? 1u : item.chance);
                         });
}

int64_t encodeRelativePosition(const Domain::Position &position) {
  return (static_cast<int64_t>(position.x) << 32) ^
         (static_cast<int64_t>(position.y) << 16) ^
         static_cast<uint16_t>(position.z);
}

void appendLayoutTile(DoodadBrush::DoodadLayout &layout,
                      Services::Preview::PreviewTileData tile) {
  const auto existing = std::find_if(
      layout.begin(), layout.end(),
      [&tile](const Services::Preview::PreviewTileData &candidate) {
        return candidate.relativePosition == tile.relativePosition;
      });

  if (existing == layout.end()) {
    layout.push_back(std::move(tile));
    return;
  }

  existing->items.insert(existing->items.end(), tile.items.begin(), tile.items.end());
}

void rebuildNeighborBrushes(Domain::ChunkedMap &map, BrushRegistry &registry,
                            const Domain::Position &center) {
  std::unordered_set<const IBrush *> rebuiltBrushes;

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const Domain::Position pos(center.x + dx, center.y + dy, center.z);
      const auto *tile = map.getTile(pos);
      if (!tile) {
        continue;
      }

      const auto rebuildBrush = [&](const Domain::Item *item) {
        if (!item) {
          return;
        }

        for (auto *brush : registry.getBrushesForItem(item->getServerId())) {
          if (!brush || !rebuiltBrushes.insert(brush).second) {
            continue;
          }

          if (auto *groundBrush = dynamic_cast<GroundBrush *>(brush)) {
            groundBrush->rebuildAround(map, pos);
          } else if (auto *wallBrush = dynamic_cast<WallBrush *>(brush)) {
            wallBrush->rebuildAround(map, pos);
          } else if (auto *tableBrush = dynamic_cast<TableBrush *>(brush)) {
            tableBrush->rebuildAround(map, pos);
          } else if (auto *carpetBrush = dynamic_cast<CarpetBrush *>(brush)) {
            carpetBrush->rebuildAround(map, pos);
          }
        }
      };

      rebuildBrush(tile->getGround());
      for (const auto &item : tile->getItems()) {
        rebuildBrush(item.get());
      }
    }
  }
}

} // namespace

DoodadBrush::DoodadBrush(std::string name, uint32_t lookId,
                         BrushRegistry &registry, bool draggable)
    : BrushBase(std::move(name), lookId, draggable), registry_(registry) {}

void DoodadBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                       const DrawContext &ctx) {
  if (!tile) {
    return;
  }

  const auto layout =
      buildPlacementLayout(tile->getPosition(), ctx.brushSettings,
                           static_cast<size_t>(ctx.variation), &map,
                           ctx.forcePlace);
  if (layout.empty()) {
    return;
  }

  applyPlacementLayout(map, tile->getPosition(), layout, ctx);
}

void DoodadBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }

  bool removedAny = false;
  if (tile->getGround() && ownsItem(tile->getGround())) {
    tile->removeGround();
    removedAny = true;
  }

  removedAny = tile->removeItemsIf([this](const Domain::Item *item) {
                 return ownsItem(item);
               }) > 0 ||
               removedAny;

  if (!removedAny || !redoBorders_) {
    return;
  }

  rebuildNeighborBrushes(map, registry_, tile->getPosition());
}

bool DoodadBrush::ownsItem(const Domain::Item *item) const {
  if (!item) {
    return false;
  }

  const auto brushId = registry_.getBrushId(this);
  return (brushId != InvalidBrushId && item->getOwnerBrushId() == brushId) ||
         ownedItemIds_.contains(item->getServerId());
}

void DoodadBrush::addAlternative(DoodadAlternative alternative) {
  for (const auto &single : alternative.getSingleItems()) {
    ownedItemIds_.insert(static_cast<uint16_t>(single.itemId));
    registry_.registerItemBinding(static_cast<uint16_t>(single.itemId), this);
    if (lookId_ == 0) {
      lookId_ = single.itemId;
    }
  }

  for (const auto &composite : alternative.getComposites()) {
    for (const auto &tile : composite.tiles) {
      for (const auto &item : tile.items) {
        ownedItemIds_.insert(static_cast<uint16_t>(item.itemId));
        registry_.registerItemBinding(static_cast<uint16_t>(item.itemId), this);
        if (lookId_ == 0) {
          lookId_ = item.itemId;
        }
      }
    }
  }

  alternatives_.push_back(std::move(alternative));
}

uint16_t DoodadBrush::getPreviewItemId() const {
  return static_cast<uint16_t>(lookId_);
}

DoodadBrush::DoodadLayout
DoodadBrush::buildPreviewTiles(const Domain::Position &anchor,
                               const Services::BrushSettingsService *brushSettings) const {
  return buildPlacementLayout(anchor, brushSettings, activeVariation_);
}

DoodadBrush::DoodadLayout
DoodadBrush::buildPlacementLayout(const Domain::Position &center,
                                  const Services::BrushSettingsService *brushSettings,
                                  size_t preferredVariation,
                                  const Domain::ChunkedMap *map,
                                  bool forcePlace) const {
  DoodadLayout layout;
  if (alternatives_.empty()) {
    return layout;
  }

  const auto *alternative = selectAlternative(preferredVariation);
  if (!alternative) {
    return layout;
  }

  std::vector<std::pair<int, int>> anchors{{0, 0}};
  if (!oneSize_ && brushSettings) {
    anchors = brushSettings->getBrushOffsets();
    if (anchors.empty()) {
      anchors.emplace_back(0, 0);
    }
  }

  std::unordered_set<int64_t> occupied;
  const auto tryPlaceCandidate = [&](const DoodadLayout &candidate) -> bool {
    if (candidate.empty()) {
      return false;
    }

    for (const auto &candidateTile : candidate) {
      if (occupied.contains(encodeRelativePosition(candidateTile.relativePosition))) {
        return false;
      }

      if (!map || forcePlace) {
        continue;
      }

      const Domain::Position targetPos(center.x + candidateTile.relativePosition.x,
                                       center.y + candidateTile.relativePosition.y,
                                       static_cast<int16_t>(
                                           center.z + candidateTile.relativePosition.z));
      const auto *targetTile = map->getTile(targetPos);
      if (!onBlocking_ && Types::tileHasBlockingContents(targetTile)) {
        return false;
      }
      if (!onDuplicate_ && tileHasOwnItem(targetTile)) {
        return false;
      }
    }

    for (const auto &candidateTile : candidate) {
      occupied.insert(encodeRelativePosition(candidateTile.relativePosition));
      appendLayoutTile(layout, candidateTile);
    }

    return true;
  };

  const auto buildSingleCandidate = [&](int anchorX, int anchorY)
      -> DoodadLayout {
    DoodadLayout candidate;
    const auto item = alternative->selectRandomSingle();
    if (item.itemId == 0) {
      return candidate;
    }

    Services::Preview::PreviewTileData tile(anchorX, anchorY, 0);
    tile.addItem(item.itemId, static_cast<uint16_t>(item.subtype));
    candidate.push_back(std::move(tile));
    return candidate;
  };

  const auto buildCompositeCandidate = [&](int anchorX, int anchorY)
      -> DoodadLayout {
    DoodadLayout candidate;
    const auto *composite = alternative->selectRandomComposite();
    if (!composite) {
      return candidate;
    }

    for (const auto &offset : composite->tiles) {
      Services::Preview::PreviewTileData tile(anchorX + offset.dx,
                                              anchorY + offset.dy, offset.dz);
      for (const auto &item : offset.items) {
        if (item.itemId != 0) {
          tile.addItem(item.itemId, static_cast<uint16_t>(item.subtype));
        }
      }
      appendLayoutTile(candidate, std::move(tile));
    }

    return candidate;
  };

  const auto singleChance = totalSingleChance(*alternative);
  const auto compositeChance = totalCompositeChance(*alternative);
  const auto totalChance = singleChance + compositeChance;
  const auto scatterMode = !oneSize_ && anchors.size() > 1;
  const auto maxAttempts =
      std::max<size_t>(1, alternative->getSingleItems().size() +
                              alternative->getComposites().size()) *
      2;

  for (const auto &[anchorX, anchorY] : anchors) {
    if (scatterMode && !WeightedSelection::passesThicknessCheck(thickness_)) {
      continue;
    }

    for (size_t attempt = 0; attempt < maxAttempts; ++attempt) {
      const bool useComposite =
          compositeChance > 0 &&
          (singleChance == 0 ||
           WeightedSelection::randomRange(1, totalChance) > singleChance);
      auto candidate = useComposite ? buildCompositeCandidate(anchorX, anchorY)
                                    : buildSingleCandidate(anchorX, anchorY);
      if (tryPlaceCandidate(candidate)) {
        break;
      }

      if (attempt + 1 == maxAttempts && useComposite && singleChance > 0) {
        candidate = buildSingleCandidate(anchorX, anchorY);
        tryPlaceCandidate(candidate);
      } else if (attempt + 1 == maxAttempts && !useComposite &&
                 compositeChance > 0) {
        candidate = buildCompositeCandidate(anchorX, anchorY);
        tryPlaceCandidate(candidate);
      }
    }
  }

  return layout;
}

std::vector<Domain::Position>
DoodadBrush::getPlacementPositions(const Domain::Position &center,
                                   const Services::BrushSettingsService *brushSettings,
                                   size_t preferredVariation,
                                   const Domain::ChunkedMap *map,
                                   bool forcePlace) const {
  std::vector<Domain::Position> positions;
  std::unordered_set<int64_t> uniquePositions;

  for (const auto &tile :
       buildPlacementLayout(center, brushSettings, preferredVariation, map,
                            forcePlace)) {
    const Domain::Position absolutePosition(
        center.x + tile.relativePosition.x, center.y + tile.relativePosition.y,
        static_cast<int16_t>(center.z + tile.relativePosition.z));
    if (!uniquePositions.insert(encodeRelativePosition(absolutePosition)).second) {
      continue;
    }

    positions.push_back(absolutePosition);
  }

  return positions;
}

void DoodadBrush::applyPlacementLayout(Domain::ChunkedMap &map,
                                       const Domain::Position &center,
                                       const DoodadLayout &layout,
                                       const DrawContext &ctx) const {
  std::vector<Domain::Position> touchedPositions;
  std::unordered_set<int64_t> uniqueTouchedPositions;

  for (const auto &layoutTile : layout) {
    const Domain::Position absolutePosition(
        center.x + layoutTile.relativePosition.x,
        center.y + layoutTile.relativePosition.y,
        static_cast<int16_t>(center.z + layoutTile.relativePosition.z));
    auto *targetTile = map.getOrCreateTile(absolutePosition);
    if (!targetTile) {
      continue;
    }

    if (removeOptionalBorder_ && targetTile->hasOptionalBorder()) {
      targetTile->setOptionalBorder(false);
      targetTile->markDirty();
    }

    for (const auto &previewItem : layoutTile.items) {
      if (previewItem.itemId == 0) {
        continue;
      }

      auto item = Types::createTypedItem(ctx, static_cast<uint16_t>(previewItem.itemId),
                                         previewItem.subtype);
      if (!item) {
        continue;
      }

      const auto *itemType = item->getType();
      if (itemType && itemType->is_ground) {
        targetTile->setGround(std::move(item));
      } else {
        if (itemType && itemType->is_wall) {
          const auto serverId = item->getServerId();
          const auto ownerBrushId = item->getOwnerBrushId();
          targetTile->removeItemsIf([serverId, ownerBrushId](const Domain::Item *existing) {
            if (!existing) {
              return false;
            }
            const auto *existingType = existing->getType();
            if (!existingType || !existingType->is_wall) {
              return false;
            }
            if (existing->getServerId() == serverId) {
              return true;
            }
            return ownerBrushId != InvalidBrushId &&
                   existing->getOwnerBrushId() == ownerBrushId;
          });
        }
        targetTile->addItem(std::move(item));
      }
    }

    if (uniqueTouchedPositions.insert(encodeRelativePosition(absolutePosition)).second) {
      touchedPositions.push_back(absolutePosition);
    }
  }

  if (!redoBorders_) {
    return;
  }

  for (const auto &position : touchedPositions) {
    rebuildNeighborBrushes(map, registry_, position);
  }
}

const DoodadAlternative *DoodadBrush::selectAlternative(size_t preferredIndex) const {
  if (alternatives_.empty()) {
    return nullptr;
  }
  if (preferredIndex < alternatives_.size()) {
    return &alternatives_[preferredIndex];
  }

  const auto index =
      WeightedSelection::randomRange(0, static_cast<uint32_t>(alternatives_.size() - 1));
  return &alternatives_[index];
}

bool DoodadBrush::tileHasOwnItem(const Domain::Tile *tile) const {
  if (!tile) {
    return false;
  }
  if (tile->getGround() && ownsItem(tile->getGround())) {
    return true;
  }
  for (const auto &item : tile->getItems()) {
    if (item && ownsItem(item.get())) {
      return true;
    }
  }
  return false;
}

} // namespace MapEditor::Brushes
