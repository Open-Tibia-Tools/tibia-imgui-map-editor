#include "HouseExitBrush.h"

#include "Domain/ChunkedMap.h"
#include "Domain/House.h"
#include "Domain/Tile.h"

namespace MapEditor::Brushes {

HouseExitBrush::HouseExitBrush()
    : BrushBase("House Exit", 0, false) {}

void HouseExitBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                          const DrawContext &ctx) {
  if (!tile || !tile->hasGround() || tile->isHouseTile() || houseId_ == 0) {
    return;
  }

  if (auto *house = map.getHouse(houseId_)) {
    if (house->entry_position != Domain::Position{}) {
      if (auto *previousTile = map.getTile(house->entry_position)) {
        previousTile->setHouseExitBrushId(InvalidBrushId);
        previousTile->setHouseExitHouseId(0);
      }
    }
    house->entry_position = tile->getPosition();
    tile->setHouseExitBrushId(ctx.ownerBrushId);
    tile->setHouseExitHouseId(houseId_);
    map.markChanged();
  }
}

void HouseExitBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile || houseId_ == 0) {
    return;
  }

  if (auto *house = map.getHouse(houseId_)) {
    if (house->entry_position == tile->getPosition()) {
      house->entry_position = {};
      tile->setHouseExitBrushId(InvalidBrushId);
      tile->setHouseExitHouseId(0);
      map.markChanged();
    }
  }
}

} // namespace MapEditor::Brushes
