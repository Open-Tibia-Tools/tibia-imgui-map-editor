#pragma once

#include "Brushes/Core/BrushBase.h"
#include "Brushes/Enums/BrushEnums.h"
#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace MapEditor::Brushes {

class BrushRegistry;

class TableBrush : public BrushBase {
public:
  TableBrush(std::string name, uint32_t lookId, BrushRegistry &registry);

  BrushType getType() const override { return BrushType::Table; }
  bool needsBorderUpdate() const override { return true; }

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;
  bool ownsItem(const Domain::Item *item) const override;

  void addAlignedItem(TableAlign align, uint16_t itemId, uint32_t chance);
  uint16_t getPreviewItemId() const;
  void rebuildAround(Domain::ChunkedMap &map, const Domain::Position &center) const;

private:
  uint16_t selectItem(TableAlign align) const;
  void rebuildTile(Domain::ChunkedMap &map, const Domain::Position &pos) const;
  bool tileHasBrush(const Domain::Tile *tile) const;

  BrushRegistry &registry_;
  std::array<std::vector<std::pair<uint16_t, uint32_t>>, 7> itemsByAlign_{};
  std::unordered_set<uint16_t> ownedItemIds_;
};

} // namespace MapEditor::Brushes
