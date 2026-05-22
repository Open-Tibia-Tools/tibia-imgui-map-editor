#pragma once

#include "Brushes/Core/BrushBase.h"

namespace MapEditor::Brushes {

class HouseExitBrush : public BrushBase {
public:
  HouseExitBrush();

  BrushType getType() const override { return BrushType::HouseExit; }
  bool isDraggable() const override { return false; }

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;

  void setHouseId(uint32_t houseId) { houseId_ = houseId; }
  uint32_t getHouseId() const { return houseId_; }

private:
  uint32_t houseId_ = 0;
};

} // namespace MapEditor::Brushes
