#include "TableBrush.h"

#include "Brushes/BrushRegistry.h"
#include "BrushUtils.h"
#include "Brushes/Behaviors/WeightedSelection.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"
#include "Services/Brushes/TableLookupService.h"
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
                                          const TableBrush &brush) {
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

TableBrush::TableBrush(std::string name, uint32_t lookId,
                       BrushRegistry &registry)
    : BrushBase(std::move(name), lookId, true), registry_(registry) {}

void TableBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                      const DrawContext &ctx) {
  if (!tile) {
    return;
  }
  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });

  const auto itemId = selectItem(TableAlign::Alone);
  if (itemId != 0) {
    tile->addItem(Types::createTypedItem(ctx, itemId));
  }
  rebuildAround(map, tile->getPosition());
}

void TableBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }
  tile->removeItemsIf([this](const Domain::Item *item) { return ownsItem(item); });
  rebuildAround(map, tile->getPosition());
}

bool TableBrush::ownsItem(const Domain::Item *item) const {
  return item && ownedItemIds_.contains(item->getServerId());
}

void TableBrush::addAlignedItem(TableAlign align, uint16_t itemId,
                                uint32_t chance) {
  itemsByAlign_[static_cast<size_t>(align)].emplace_back(itemId,
                                                         chance == 0 ? 1u : chance);
  ownedItemIds_.insert(itemId);
  registry_.registerItemBinding(itemId, this);
  if (lookId_ == 0) {
    lookId_ = itemId;
  }
}

uint16_t TableBrush::getPreviewItemId() const {
  const auto horizontal = selectItem(TableAlign::Horizontal);
  return horizontal != 0 ? horizontal : lookId_;
}

void TableBrush::rebuildAround(Domain::ChunkedMap &map,
                               const Domain::Position &center) const {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      rebuildTile(map, {center.x + dx, center.y + dy, center.z});
    }
  }
}

uint16_t TableBrush::selectItem(TableAlign align) const {
  const auto &items = itemsByAlign_[static_cast<size_t>(align)];
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

void TableBrush::rebuildTile(Domain::ChunkedMap &map,
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

  const auto align = Services::Brushes::TableLookupService{}.getTableType(neighbors);
  auto itemId = selectItem(align);
  if (itemId == 0) {
    itemId = selectItem(TableAlign::Alone);
  }
  if (itemId == 0) {
    return;
  }

  const auto ownerBrushId = registry_.getBrushId(this);
  auto ownedItems = getOwnedItems(*tile, *this);
  if (!ownedItems.empty()) {
    Types::updateItemVisuals(*ownedItems.front(), registry_, itemId,
                             ownerBrushId);
    tile->removeItemsIf([this, keep = ownedItems.front()](const Domain::Item *item) {
      return ownsItem(item) && item != keep;
    });
    return;
  }

  DrawContext ctx;
  ctx.clientData = registry_.getClientDataService();
  ctx.brushRegistry = &registry_;
  ctx.ownerBrushId = ownerBrushId;
  tile->addItem(Types::createTypedItem(ctx, itemId));
}

bool TableBrush::tileHasBrush(const Domain::Tile *tile) const {
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
