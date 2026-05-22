#include "OptionalBorderBrush.h"

#include "Brushes/BrushRegistry.h"
#include "GroundBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"

namespace MapEditor::Brushes {

OptionalBorderBrush::OptionalBorderBrush()
    : BrushBase("Optional Border Tool", 0, true) {}

void OptionalBorderBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                               const DrawContext &ctx) {
  if (!tile || !tile->hasGround() || !ctx.brushRegistry) {
    return;
  }

  tile->setOptionalBorder(true);
  tile->setOptionalBorderBrushId(ctx.ownerBrushId);

  if (auto *brush = dynamic_cast<GroundBrush *>(
          ctx.brushRegistry->getBrushForItem(tile->getGround()->getServerId()));
      brush && brush->hasOptionalBorderRule()) {
    brush->rebuildAround(map, tile->getPosition());
  }

  map.markChanged();
}

void OptionalBorderBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile || !tile->hasGround()) {
    return;
  }
  tile->setOptionalBorder(false);
  tile->setOptionalBorderBrushId(InvalidBrushId);
  map.markChanged();
}

} // namespace MapEditor::Brushes
