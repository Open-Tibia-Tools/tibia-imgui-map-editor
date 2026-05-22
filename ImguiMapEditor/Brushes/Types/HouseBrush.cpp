#include "HouseBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

HouseBrush::HouseBrush() = default;

void HouseBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                      const DrawContext &ctx) {
  if (!tile)
    return;
  if (houseId_ == 0)
    return; // No house selected

  if (!map.getHouse(houseId_)) {
    auto house = std::make_unique<Domain::House>(houseId_);
    house->name = "House " + std::to_string(houseId_);
    map.addHouse(std::move(house));
  }

  tile->setHouseId(houseId_);
  tile->setHouseBrushId(ctx.ownerBrushId);
  tile->addFlag(Domain::TileFlag::ProtectionZone);
  map.markChanged();

  spdlog::trace("[HouseBrush] Set house ID {} at ({},{},{})", houseId_,
                tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

void HouseBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile)
    return;

  tile->setHouseId(0); // Clear house assignment
  tile->setHouseBrushId(InvalidBrushId);
  tile->removeFlag(Domain::TileFlag::ProtectionZone);
  map.markChanged();

  spdlog::trace("[HouseBrush] Cleared house from ({},{},{})",
                tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

} // namespace MapEditor::Brushes
