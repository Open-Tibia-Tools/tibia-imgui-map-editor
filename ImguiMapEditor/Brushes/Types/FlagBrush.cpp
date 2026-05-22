#include "FlagBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

FlagBrush::FlagBrush(Domain::TileFlag flag, const std::string &name)
    : flag_(flag), name_(name) {}

void FlagBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                     const DrawContext &ctx) {
  if (!tile)
    return;

  // Add the flag to this tile
  tile->addFlag(flag_);
  tile->setZoneBrushId(flag_, ctx.ownerBrushId);
  map.markChanged();

  spdlog::trace("[FlagBrush] Added {} flag to ({},{},{})", name_,
                tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

void FlagBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile)
    return;

  // Remove the flag from this tile
  tile->removeFlag(flag_);
  tile->setZoneBrushId(flag_, InvalidBrushId);
  map.markChanged();

  spdlog::trace("[FlagBrush] Removed {} flag from ({},{},{})", name_,
                tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

} // namespace MapEditor::Brushes
