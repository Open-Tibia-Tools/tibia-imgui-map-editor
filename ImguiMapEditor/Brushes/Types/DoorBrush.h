#pragma once

#include "Brushes/Core/BrushBase.h"
#include "Brushes/Enums/BrushEnums.h"

namespace MapEditor::Brushes {

class BrushRegistry;
class WallBrush;

class DoorBrush : public BrushBase {
public:
  DoorBrush(std::string name, uint32_t lookId, DoorType doorType,
            BrushRegistry &registry);

  BrushType getType() const override { return BrushType::Door; }
  bool canDraw(const Domain::ChunkedMap &map,
               const Domain::Position &pos) const override;

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;

  DoorType getDoorType() const { return doorType_; }
  void setOpen(bool open) { open_ = open; }
  bool isOpen() const { return open_; }

private:
  WallBrush *findWallBrush(const Domain::Tile *tile) const;

  DoorType doorType_;
  BrushRegistry &registry_;
  bool open_ = false;
};

} // namespace MapEditor::Brushes
