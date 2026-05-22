#pragma once

#include "WallBrush.h"

namespace MapEditor::Brushes {

/* Wall decoration brush that reuses wall loading and placement behavior. */
class WallDecorationBrush final : public WallBrush {
public:
  using WallBrush::WallBrush;

  BrushType getType() const override { return BrushType::WallDecoration; }
  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;
};

} // namespace MapEditor::Brushes
