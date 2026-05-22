#include "CarpetBrush.h"

#include "Brushes/BrushRegistry.h"
#include "BrushUtils.h"
#include "Brushes/Behaviors/WeightedSelection.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"
#include "Services/Brushes/CarpetLookupService.h"
#include <array>

namespace MapEditor::Brushes {

namespace {

constexpr std::array<std::tuple<int, int, TileNeighbor>, 8> kNeighborOffsets{{
    {-1, -1, TileNeighbor::Northwest},
    {0, -1, TileNeighbor::North},
    {1, -1, TileNeighbor::Northeast},
    {-1, 0, TileNeighbor::West},
    {1, 0, TileNeighbor::East},
    {-1, 1, TileNeighbor::Southwest},
    {0, 1, TileNeighbor::South},
    {1, 1, TileNeighbor::Southeast},
}};

std::vector<Domain::Item *> getOwnedItems(Domain::Tile &tile,
                                          const CarpetBrush &brush) {
  std::vector<Domain::Item *> items;
  items.reserve(tile.getItemCount());

  for (const auto &item : tile.getItems()) {
    if (item && brush.ownsItem(item.get())) {
      items.push_back(item.get());
    }
  }

  return items;
}

} // namespace

CarpetBrush::CarpetBrush(std::string name, uint32_t lookId,
                         BrushRegistry &registry)
    : BrushBase(std::move(name), lookId, true), registry_(registry) {}

void CarpetBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                       const DrawContext &ctx) {
  if (!tile) {
    return;
  }

  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
  const auto centerId = selectItem(EdgeType::Center);
  if (centerId != 0) {
    tile->addItem(Types::createTypedItem(ctx, centerId));
  }
  rebuildAround(map, tile->getPosition());
}

void CarpetBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }
  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
  rebuildAround(map, tile->getPosition());
}

bool CarpetBrush::ownsItem(const Domain::Item *item) const {
  return item && ownedItemIds_.contains(item->getServerId());
}

void CarpetBrush::addAlignedItem(EdgeType align, uint16_t itemId,
                                 uint32_t chance) {
  itemsByEdge_[static_cast<size_t>(align)].emplace_back(itemId,
                                                        chance == 0 ? 1u : chance);
  ownedItemIds_.insert(itemId);
  registry_.registerItemBinding(itemId, this);
  if (lookId_ == 0) {
    lookId_ = itemId;
  }
}

uint16_t CarpetBrush::getPreviewItemId() const {
  const auto center = selectItem(EdgeType::Center);
  return center != 0 ? center : lookId_;
}

void CarpetBrush::rebuildAround(Domain::ChunkedMap &map,
                                const Domain::Position &center) const {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      rebuildTile(map, {center.x + dx, center.y + dy, center.z});
    }
  }
}

uint16_t CarpetBrush::selectItem(EdgeType align) const {
  const auto &items = itemsByEdge_[static_cast<size_t>(align)];
  if (items.empty()) {
    return 0;
  }
  std::vector<uint32_t> weights;
  weights.reserve(items.size());
  for (const auto &[_, weight] : items) {
    weights.push_back(weight == 0 ? 1u : weight);
  }
  const auto index = WeightedSelection::select(weights);
  return index ? items[*index].first : items.front().first;
}

void CarpetBrush::rebuildTile(Domain::ChunkedMap &map,
                              const Domain::Position &pos) const {
  auto *tile = map.getTile(pos);
  if (!tile || !tileHasBrush(tile)) {
    return;
  }

  TileNeighbor neighbors = TileNeighbor::None;
  for (const auto &[dx, dy, bit] : kNeighborOffsets) {
    const auto *neighborTile = map.getTile(pos.x + dx, pos.y + dy, pos.z);
    if (tileHasBrush(neighborTile)) {
      neighbors |= bit;
    }
  }

  const auto packed =
      Services::Brushes::CarpetLookupService{}.getCarpetTypes(neighbors);
  auto types = Services::Brushes::CarpetLookupService::unpack(packed);
  if (types.empty()) {
    types.push_back(EdgeType::Center);
  }

  const auto ownerBrushId = registry_.getBrushId(this);
  auto ownedItems = getOwnedItems(*tile, *this);
  size_t itemIndex = 0;

  for (const auto edge : types) {
    const auto itemId = selectItem(edge);
    if (itemId == 0) {
      continue;
    }

    if (itemIndex < ownedItems.size()) {
      Types::updateItemVisuals(*ownedItems[itemIndex], registry_, itemId,
                               ownerBrushId);
    } else {
      DrawContext ctx;
      ctx.clientData = registry_.getClientDataService();
      ctx.brushRegistry = &registry_;
      ctx.ownerBrushId = ownerBrushId;
      tile->addItem(Types::createTypedItem(ctx, itemId));
    }

    ++itemIndex;
  }

  if (itemIndex == 0) {
    if (const auto centerId = selectItem(EdgeType::Center); centerId != 0) {
      DrawContext ctx;
      ctx.clientData = registry_.getClientDataService();
      ctx.brushRegistry = &registry_;
      ctx.ownerBrushId = ownerBrushId;
      tile->addItem(Types::createTypedItem(ctx, centerId));
      itemIndex = 1;
    }
  }

  if (itemIndex < ownedItems.size()) {
    size_t seen = 0;
    tile->removeItemsIf([this, &seen, itemIndex](const Domain::Item *item) mutable {
      if (!ownsItem(item)) {
        return false;
      }

      const bool remove = seen >= itemIndex;
      ++seen;
      return remove;
    });
  }
}

bool CarpetBrush::tileHasBrush(const Domain::Tile *tile) const {
  if (!tile) {
    return false;
  }

  for (const auto &item : tile->getItems()) {
    if (item && ownsItem(item.get())) {
      return true;
    }
  }
  return false;
}

} // namespace MapEditor::Brushes
