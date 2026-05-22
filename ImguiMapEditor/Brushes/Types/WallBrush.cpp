#include "WallBrush.h"

#include "Brushes/BrushRegistry.h"
#include "Brushes/Types/BrushUtils.h"
#include "Brushes/Types/WallDecorationBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"
#include "Services/ClientDataService.h"
#include "Services/Brushes/WallLookupService.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <unordered_set>

namespace MapEditor::Brushes {

namespace {

std::string normalizeName(const std::string &value) {
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return normalized;
}

constexpr std::array<std::tuple<int, int, WallNeighbor>, 4> kWallNeighbors{{
    {0, -1, WallNeighbor::North},
    {-1, 0, WallNeighbor::West},
    {1, 0, WallNeighbor::East},
    {0, 1, WallNeighbor::South},
}};

template <typename Predicate>
WallAlign computeWallAlignment(const Domain::ChunkedMap &map,
                               const Domain::Position &pos,
                               Predicate &&hasWallGroup) {
  WallNeighbor neighbors = WallNeighbor::None;
  for (const auto &[dx, dy, bit] : kWallNeighbors) {
    const auto *neighborTile = map.getTile(pos.x + dx, pos.y + dy, pos.z);
    if (std::invoke(hasWallGroup, neighborTile)) {
      neighbors |= bit;
    }
  }

  return Services::Brushes::WallLookupService{}.getFullType(neighbors);
}

template <typename Fn>
bool visitWallRedirectChain(const WallBrush &root, Fn &&fn) {
  std::vector<const WallBrush *> pending{&root};
  std::unordered_set<const WallBrush *> visited;

  while (!pending.empty()) {
    const auto *brush = pending.back();
    pending.pop_back();
    if (!brush || !visited.insert(brush).second) {
      continue;
    }

    if (std::invoke(fn, *brush)) {
      return true;
    }

    for (const auto *redirectBrush : brush->getRedirectBrushes()) {
      if (redirectBrush && !visited.contains(redirectBrush)) {
        pending.push_back(redirectBrush);
      }
    }
  }

  return false;
}

const WallBrush *resolveWallBrushForItem(const Domain::Item &item,
                                         BrushRegistry &registry) {
  if (item.getOwnerBrushId() != InvalidBrushId) {
    if (const auto *brush =
            dynamic_cast<const WallBrush *>(registry.getBrushById(item.getOwnerBrushId()))) {
      return brush;
    }
  }

  for (auto *brush : registry.getBrushesForItem(item.getServerId())) {
    if (const auto *wallBrush = dynamic_cast<const WallBrush *>(brush)) {
      return wallBrush;
    }
  }

  return nullptr;
}

Domain::Item *resolveDoorItem(Domain::Tile &tile,
                              const Domain::Item *preferredItem,
                              const WallBrush &brush) {
  if (preferredItem) {
    for (const auto &item : tile.getItems()) {
      if (item.get() == preferredItem &&
          brush.findDoorForItem(item->getServerId()).has_value()) {
        return item.get();
      }
    }
  }

  for (auto it = tile.getItems().rbegin(); it != tile.getItems().rend(); ++it) {
    if (*it && brush.findDoorForItem((*it)->getServerId()).has_value()) {
      return it->get();
    }
  }

  return nullptr;
}

template <typename Resolver>
void updateConsecutiveDecorations(Domain::Tile &tile, Domain::Item *baseItem,
                                  BrushRegistry &registry,
                                  Resolver &&resolveItemId) {
  if (!baseItem) {
    return;
  }

  size_t baseIndex = tile.getItemCount();
  for (size_t index = 0; index < tile.getItemCount(); ++index) {
    if (tile.getItem(index) == baseItem) {
      baseIndex = index;
      break;
    }
  }

  if (baseIndex == tile.getItemCount()) {
    return;
  }

  std::vector<const Domain::Item *> itemsToRemove;
  for (size_t index = baseIndex + 1; index < tile.getItemCount(); ++index) {
    auto *item = tile.getItem(index);
    if (!item) {
      continue;
    }

    const auto *itemBrush = resolveWallBrushForItem(*item, registry);
    auto *decorationBrush = dynamic_cast<const WallBrush *>(itemBrush);
    if (!decorationBrush ||
        decorationBrush->getType() != BrushType::WallDecoration) {
      break;
    }

    if (const auto itemId = std::invoke(resolveItemId, *decorationBrush, *item);
        itemId != 0) {
      const auto ownerBrushId =
          item->getOwnerBrushId() != InvalidBrushId
              ? item->getOwnerBrushId()
              : registry.getBrushId(decorationBrush);
      Types::updateItemVisuals(*item, registry, itemId, ownerBrushId);
      continue;
    }

    itemsToRemove.push_back(item);
  }

  if (!itemsToRemove.empty()) {
    tile.removeItemsIf([&itemsToRemove](const Domain::Item *item) {
      return std::find(itemsToRemove.begin(), itemsToRemove.end(), item) !=
             itemsToRemove.end();
    });
  }
}

} // namespace

WallBrush::WallBrush(std::string name, uint32_t lookId, BrushRegistry &registry)
    : BrushBase(std::move(name), lookId, true), registry_(registry) {}

void WallBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                     const DrawContext &ctx) {
  if (!tile) {
    return;
  }

  if (ctx.specialAction) {
    for (const auto &item : tile->getItems()) {
      if (!item) {
        continue;
      }

      const auto *itemBrush = resolveWallBrushForItem(*item, registry_);
      if (itemBrush != this) {
        continue;
      }

      const auto replacementId = findNextWallVariant(item->getServerId());
      if (!replacementId.has_value()) {
        return;
      }

      Types::updateItemVisuals(*item, registry_, *replacementId,
                               ctx.ownerBrushId != InvalidBrushId
                                   ? ctx.ownerBrushId
                                   : item->getOwnerBrushId());
      tile->markDirty();
      return;
    }
  }

  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
  if (const auto itemId = selectWallItem(WallAlign::Horizontal); itemId != 0) {
    tile->addItem(Types::createTypedItem(ctx, itemId));
  }
  const bool deferNeighborRebuild =
      ctx.forcePlace && ctx.isDragging && !ctx.specialAction;
  if (!deferNeighborRebuild) {
    rebuildAround(map, tile->getPosition());
  }
}

void WallBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }
  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
  rebuildAround(map, tile->getPosition());
}

bool WallBrush::ownsItem(const Domain::Item *item) const {
  return item && ownedItemIds_.contains(item->getServerId());
}

void WallBrush::addWallItem(WallAlign align, uint16_t itemId, uint32_t chance) {
  wallNodes_[static_cast<size_t>(align)].addItem(itemId, chance == 0 ? 1u : chance);
  ownedItemIds_.insert(itemId);
  registry_.registerItemBinding(itemId, this);
  if (lookId_ == 0) {
    lookId_ = itemId;
  }
}

void WallBrush::addDoorItem(WallAlign align, DoorNode door) {
  for (const auto itemId : door.items) {
    ownedItemIds_.insert(itemId);
    registry_.registerItemBinding(itemId, this);
    if (lookId_ == 0) {
      lookId_ = itemId;
    }
  }
  door.alignment = align;
  doorNodes_[static_cast<size_t>(align)].push_back(std::move(door));
}

void WallBrush::addRedirectName(const std::string &name) {
  if (!name.empty()) {
    redirectNames_.insert(normalizeName(name));
  }
}

uint16_t WallBrush::getPreviewItemId() const {
  if (const auto itemId = selectWallItem(WallAlign::Horizontal); itemId != 0) {
    return itemId;
  }
  return lookId_;
}

std::optional<WallAlign> WallBrush::getAlignmentForItem(uint16_t itemId) const {
  return findAlignmentForItem(itemId);
}

uint16_t WallBrush::getWallItemForAlign(WallAlign align) const {
  return selectWallItem(align);
}

std::optional<DoorNode> WallBrush::getDoorItemForAlign(WallAlign align,
                                                       DoorType type, bool open,
                                                       bool preferLocked) const {
  return selectDoorItem(align, type, open, preferLocked);
}

void WallBrush::rebuildAround(Domain::ChunkedMap &map,
                              const Domain::Position &center) const {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      rebuildTile(map, {center.x + dx, center.y + dy, center.z});
    }
  }
}

void WallBrush::rebuildNeighbors(Domain::ChunkedMap &map,
                                 const Domain::Position &center) const {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }

      rebuildTile(map, {center.x + dx, center.y + dy, center.z});
    }
  }
}

bool WallBrush::canApplyDoor(const Domain::Tile &tile, DoorType type, bool open,
                             bool preferLocked) const {
  for (const auto &item : tile.getItems()) {
    if (!item) {
      continue;
    }

    const auto *itemBrush = resolveWallBrushForItem(*item, registry_);
    if (!itemBrush || itemBrush->getType() == BrushType::WallDecoration ||
        !connectsTo(itemBrush)) {
      continue;
    }

    const auto alignment = findAlignmentForItem(item->getServerId());
    if (!alignment.has_value()) {
      continue;
    }

    if (selectDoorItem(*alignment, type, open, preferLocked).has_value()) {
      return true;
    }
  }

  return false;
}

bool WallBrush::applyDoor(Domain::ChunkedMap &map, Domain::Tile &tile,
                          DoorType type, bool open, bool preferLocked,
                          BrushId ownerBrushId) const {
  const auto pos = tile.getPosition();
  const auto resolvedOwnerBrushId =
      ownerBrushId != InvalidBrushId ? ownerBrushId : registry_.getBrushId(this);
  auto *baseWallItem = [&]() -> Domain::Item * {
    for (const auto &item : tile.getItems()) {
      if (!item) {
        continue;
      }

      const auto *itemBrush = resolveWallBrushForItem(*item, registry_);
      if (!itemBrush || itemBrush->getType() == BrushType::WallDecoration ||
          !connectsTo(itemBrush)) {
        continue;
      }

      if (const auto alignment = findAlignmentForItem(item->getServerId());
          alignment && selectDoorItem(*alignment, type, open, preferLocked).has_value()) {
        return item.get();
      }
    }

    return nullptr;
  }();

  if (!baseWallItem) {
    return false;
  }

  const auto alignment = findAlignmentForItem(baseWallItem->getServerId());
  if (!alignment.has_value()) {
    return false;
  }

  const auto baseDoor = selectDoorItem(*alignment, type, open, preferLocked);
  if (!baseDoor.has_value()) {
    return false;
  }

  Types::updateItemVisuals(*baseWallItem, registry_,
                           static_cast<uint16_t>(baseDoor->getItem()),
                           resolvedOwnerBrushId);

  updateConsecutiveDecorations(
      tile, baseWallItem, registry_,
      [alignment = *alignment, type, open, preferLocked](
          const WallBrush &decorationBrush, const Domain::Item &) -> uint16_t {
        if (const auto decorationDoor =
                decorationBrush.getDoorItemForAlign(alignment, type, open,
                                                    preferLocked);
            decorationDoor.has_value()) {
          return static_cast<uint16_t>(decorationDoor->getItem());
        }

        return 0;
      });

  tile.markDirty();
  rebuildAround(map, pos);
  return true;
}

bool WallBrush::removeDoor(Domain::ChunkedMap &map, Domain::Tile &tile,
                           const Domain::Item *preferredItem) const {
  auto *targetItem = resolveDoorItem(tile, preferredItem, *this);
  if (!targetItem) {
    return false;
  }

  const auto currentDoor = findDoorForItem(targetItem->getServerId());
  if (!currentDoor) {
    return false;
  }

  const auto alignment =
      findAlignmentForItem(targetItem->getServerId()).value_or(currentDoor->alignment);
  const auto replacementId = selectWallItem(alignment);
  if (replacementId == 0) {
    return false;
  }

  const auto ownerBrushId = targetItem->getOwnerBrushId() != InvalidBrushId
                                ? targetItem->getOwnerBrushId()
                                : registry_.getBrushId(this);
  Types::updateItemVisuals(*targetItem, registry_, replacementId, ownerBrushId);

  updateConsecutiveDecorations(
      tile, targetItem, registry_,
      [alignment](const WallBrush &decorationBrush,
                  const Domain::Item &) -> uint16_t {
        return decorationBrush.getWallItemForAlign(alignment);
      });

  tile.markDirty();
  rebuildAround(map, tile.getPosition());
  return true;
}

bool WallBrush::switchDoor(Domain::ChunkedMap &map, Domain::Tile &tile,
                           const Domain::Item *preferredItem,
                           bool preferLocked) const {
  auto *targetItem = resolveDoorItem(tile, preferredItem, *this);

  if (!targetItem) {
    return false;
  }

  const auto currentDoor = findDoorForItem(targetItem->getServerId());
  if (!currentDoor) {
    return false;
  }

  const auto alignment =
      findAlignmentForItem(targetItem->getServerId()).value_or(currentDoor->alignment);
  const auto replacement =
      selectDoorItem(alignment, currentDoor->type, !currentDoor->isOpen, preferLocked);
  if (!replacement || replacement->getItem() == 0) {
    return false;
  }

  const auto replacementId = static_cast<uint16_t>(replacement->getItem());
  Types::updateItemVisuals(*targetItem, registry_, replacementId,
                           targetItem->getOwnerBrushId());

  updateConsecutiveDecorations(
      tile, targetItem, registry_,
      [alignment, replacement, preferLocked](const WallBrush &decorationBrush,
                                             const Domain::Item &) -> uint16_t {
        if (const auto replacementDoor =
                decorationBrush.getDoorItemForAlign(alignment, replacement->type,
                                                    replacement->isOpen,
                                                    preferLocked);
            replacementDoor.has_value()) {
          return static_cast<uint16_t>(replacementDoor->getItem());
        }

        return 0;
      });

  tile.markDirty();
  rebuildAround(map, tile.getPosition());
  return true;
}

std::optional<DoorNode> WallBrush::findDoorForItem(uint16_t itemId) const {
  std::optional<DoorNode> foundDoor;
  visitWallRedirectChain(*this, [&](const WallBrush &brush) {
    for (const auto &doors : brush.doorNodes_) {
      for (const auto &door : doors) {
        if (std::find(door.items.begin(), door.items.end(), itemId) !=
            door.items.end()) {
          foundDoor = door;
          return true;
        }
      }
    }
    return false;
  });
  return foundDoor;
}

std::vector<const WallBrush *> WallBrush::getRedirectBrushes() const {
  std::vector<const WallBrush *> result;
  if (redirectNames_.empty()) {
    return result;
  }

  for (auto *brush : registry_.getAllBrushes()) {
    auto *wallBrush = dynamic_cast<const WallBrush *>(brush);
    if (!wallBrush || wallBrush == this) {
      continue;
    }

    if (redirectNames_.contains(normalizeName(wallBrush->getName()))) {
      result.push_back(wallBrush);
    }
  }
  return result;
}

bool WallBrush::isWallGroupItem(uint16_t itemId) const {
  for (auto *brush : registry_.getBrushesForItem(itemId)) {
    auto *wallBrush = dynamic_cast<const WallBrush *>(brush);
    if (wallBrush && connectsTo(wallBrush)) {
      return true;
    }
  }

  return findAlignmentForItem(itemId).has_value();
}

std::optional<WallAlign> WallBrush::findAlignmentForItem(uint16_t itemId) const {
  std::optional<WallAlign> alignment;
  visitWallRedirectChain(*this, [&](const WallBrush &brush) {
    for (size_t index = 0; index < brush.wallNodes_.size(); ++index) {
      const auto align = static_cast<WallAlign>(index);
      for (const auto &[candidateId, _] : brush.wallNodes_[index].getItems()) {
        if (candidateId == itemId) {
          alignment = align;
          return true;
        }
      }

      for (const auto &door : brush.doorNodes_[index]) {
        if (std::find(door.items.begin(), door.items.end(), itemId) !=
            door.items.end()) {
          alignment = align;
          return true;
        }
      }
    }
    return false;
  });
  return alignment;
}

std::optional<uint16_t> WallBrush::findNextWallVariant(uint16_t currentItemId) const {
  const auto alignment = findAlignmentForItem(currentItemId);
  if (!alignment.has_value()) {
    return std::nullopt;
  }

  std::vector<uint16_t> candidates;
  visitWallRedirectChain(*this, [&](const WallBrush &brush) {
    for (const auto &[candidateId, _] :
         brush.wallNodes_[static_cast<size_t>(*alignment)].getItems()) {
      if (candidateId == 0 ||
          std::find(candidates.begin(), candidates.end(), candidateId) !=
              candidates.end()) {
        continue;
      }

      candidates.push_back(static_cast<uint16_t>(candidateId));
    }

    return false;
  });

  if (candidates.size() <= 1) {
    return std::nullopt;
  }

  const auto currentIt =
      std::find(candidates.begin(), candidates.end(), currentItemId);
  if (currentIt == candidates.end()) {
    return candidates.front();
  }

  auto nextIt = std::next(currentIt);
  if (nextIt == candidates.end()) {
    nextIt = candidates.begin();
  }

  return *nextIt;
}

std::optional<WallAlign>
WallBrush::findTileAlignment(const Domain::Tile &tile) const {
  for (const auto &item : tile.getItems()) {
    if (!item) {
      continue;
    }
    if (const auto alignment = findAlignmentForItem(item->getServerId())) {
      return alignment;
    }
  }
  return std::nullopt;
}

uint16_t WallBrush::selectWallItem(WallAlign align) const {
  uint16_t itemId = 0;
  visitWallRedirectChain(*this, [&](const WallBrush &brush) {
    itemId = brush.wallNodes_[static_cast<size_t>(align)].getRandomItem();
    return itemId != 0;
  });
  return itemId;
}

std::optional<DoorNode> WallBrush::selectDoorItem(WallAlign align,
                                                  DoorType type, bool open,
                                                  bool preferLocked) const {
  std::optional<DoorNode> bestMatch;
  int bestRank = -1;

  visitWallRedirectChain(*this, [&](const WallBrush &brush) {
    const auto &doors = brush.doorNodes_[static_cast<size_t>(align)];

    for (const auto &door : doors) {
      if (door.type != type) {
        continue;
      }

      const int rank = (door.isOpen == open)
                           ? ((!preferLocked || door.isLocked) ? 3 : 2)
                           : 1;
      if (rank > bestRank) {
        bestMatch = door;
        bestRank = rank;
        if (bestRank == 3) {
          return true;
        }
      }
    }

    return false;
  });

  return bestMatch;
}

void WallBrush::rebuildTile(Domain::ChunkedMap &map,
                            const Domain::Position &pos) const {
  auto *tile = map.getTile(pos);
  if (!tile || !tileHasWallGroup(tile)) {
    return;
  }

  DoorType currentDoorType = DoorType::Undefined;
  bool isOpen = false;
  bool isLocked = false;
  std::optional<WallAlign> currentAlignment;
  BrushId currentOwnerBrushId = registry_.getBrushId(this);
  Domain::Item *baseItem = nullptr;
  std::vector<Domain::Item *> duplicateBaseItems;
  for (const auto &item : tile->getItems()) {
    if (!item) {
      continue;
    }

    const auto *itemBrush = resolveWallBrushForItem(*item, registry_);
    if (!itemBrush || itemBrush->getType() == BrushType::WallDecoration ||
        !connectsTo(itemBrush)) {
      continue;
    }

    if (!baseItem) {
      baseItem = item.get();
      currentAlignment = findAlignmentForItem(item->getServerId());
    } else {
      duplicateBaseItems.push_back(item.get());
    }

    if (const auto door = findDoorForItem(item->getServerId())) {
      currentDoorType = door->type;
      isOpen = door->isOpen;
      isLocked = door->isLocked;
      if (item->getOwnerBrushId() != InvalidBrushId) {
        currentOwnerBrushId = item->getOwnerBrushId();
      }
    }
  }

  if (!baseItem) {
    return;
  }

  const auto align =
      computeWallAlignment(map, pos, [this](const Domain::Tile *neighborTile) {
        return tileHasWallGroup(neighborTile);
      });

  auto resolvedAlignment = align;
  uint16_t replacementId = 0;

  if (currentDoorType != DoorType::Undefined) {
    if (currentAlignment) {
      if (const auto door =
              selectDoorItem(*currentAlignment, currentDoorType, isOpen, isLocked)) {
        replacementId = static_cast<uint16_t>(door->getItem());
        resolvedAlignment = *currentAlignment;
      }
    }

    if (replacementId == 0) {
      if (const auto door =
              selectDoorItem(align, currentDoorType, isOpen, isLocked)) {
        replacementId = static_cast<uint16_t>(door->getItem());
        resolvedAlignment = align;
      }
    }
  }

  if (replacementId == 0) {
    replacementId = selectWallItem(align);
    if (replacementId == 0) {
      replacementId = selectWallItem(WallAlign::Horizontal);
      resolvedAlignment = WallAlign::Horizontal;
    }
  }

  if (replacementId == 0) {
    return;
  }

  Types::updateItemVisuals(*baseItem, registry_, replacementId,
                           currentOwnerBrushId);

  updateConsecutiveDecorations(
      *tile, baseItem, registry_,
      [resolvedAlignment, currentDoorType, isOpen, isLocked](
          const WallBrush &decorationBrush, const Domain::Item &) -> uint16_t {
        if (currentDoorType != DoorType::Undefined) {
          if (const auto decorationDoor =
                  decorationBrush.getDoorItemForAlign(resolvedAlignment,
                                                      currentDoorType, isOpen,
                                                      isLocked);
              decorationDoor.has_value()) {
            return static_cast<uint16_t>(decorationDoor->getItem());
          }

          return 0;
        }

        return decorationBrush.getWallItemForAlign(resolvedAlignment);
      });

  if (!duplicateBaseItems.empty()) {
    tile->removeItemsIf([&duplicateBaseItems](const Domain::Item *item) {
      return std::find(duplicateBaseItems.begin(), duplicateBaseItems.end(),
                       item) != duplicateBaseItems.end();
    });
  }

  tile->markDirty();
}

bool WallBrush::connectsTo(const IBrush *brush) const {
  if (brush == this) {
    return true;
  }

  const auto *wallBrush = dynamic_cast<const WallBrush *>(brush);
  if (!wallBrush) {
    return false;
  }

  const auto otherName = normalizeName(wallBrush->getName());
  return redirectNames_.contains(otherName) ||
         wallBrush->redirectNames_.contains(normalizeName(getName()));
}

bool WallBrush::tileHasWallGroup(const Domain::Tile *tile) const {
  if (!tile) {
    return false;
  }

  for (const auto &item : tile->getItems()) {
    if (!item) {
      continue;
    }
    if (isWallGroupItem(item->getServerId())) {
      return true;
    }
  }
  return false;
}

void WallDecorationBrush::draw(Domain::ChunkedMap &, Domain::Tile *tile,
                               const DrawContext &ctx) {
  if (!tile) {
    return;
  }

  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });

  for (size_t index = 0; index < tile->getItemCount(); ++index) {
    auto *item = tile->getItem(index);
    if (!item) {
      continue;
    }

    const auto *baseBrush = resolveWallBrushForItem(*item, brushRegistry());
    if (!baseBrush || baseBrush->getType() == BrushType::WallDecoration) {
      continue;
    }

    const auto alignment = baseBrush->getAlignmentForItem(item->getServerId());
    if (!alignment.has_value()) {
      continue;
    }

    uint16_t decorationId = 0;
    if (const auto door = baseBrush->findDoorForItem(item->getServerId());
        door.has_value()) {
      if (const auto decorationDoor =
              getDoorItemForAlign(*alignment, door->type, door->isOpen,
                                  door->isLocked);
          decorationDoor.has_value()) {
        decorationId = static_cast<uint16_t>(decorationDoor->getItem());
      }
    } else {
      decorationId = getWallItemForAlign(*alignment);
    }

    if (decorationId == 0) {
      continue;
    }

    tile->insertItem(index + 1, Types::createTypedItem(ctx, decorationId));
    ++index;
  }
}

void WallDecorationBrush::undraw(Domain::ChunkedMap &, Domain::Tile *tile) {
  if (!tile) {
    return;
  }

  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
}

} // namespace MapEditor::Brushes
