#pragma once

#include "Brushes/Core/BrushBase.h"
#include "Brushes/Data/WallNode.h"
#include "Brushes/Enums/BrushEnums.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace MapEditor::Brushes {

class BrushRegistry;

class WallBrush : public BrushBase {
public:
  WallBrush(std::string name, uint32_t lookId, BrushRegistry &registry);

  BrushType getType() const override { return BrushType::Wall; }
  bool needsBorderUpdate() const override { return true; }

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;
  bool ownsItem(const Domain::Item *item) const override;

  void addWallItem(WallAlign align, uint16_t itemId, uint32_t chance);
  void addDoorItem(WallAlign align, DoorNode door);
  void addRedirectName(const std::string &name);

  uint16_t getPreviewItemId() const;
  void rebuildAround(Domain::ChunkedMap &map, const Domain::Position &center) const;

  bool canApplyDoor(const Domain::Tile &tile, DoorType type, bool open,
                    bool preferLocked) const;
  bool applyDoor(Domain::ChunkedMap &map, Domain::Tile &tile, DoorType type,
                 bool open, bool preferLocked,
                 BrushId ownerBrushId = InvalidBrushId) const;
  bool removeDoor(Domain::ChunkedMap &map, Domain::Tile &tile,
                  const Domain::Item *preferredItem = nullptr) const;
  bool switchDoor(Domain::ChunkedMap &map, Domain::Tile &tile,
                  const Domain::Item *preferredItem,
                  bool preferLocked) const;
  std::optional<WallAlign> getAlignmentForItem(uint16_t itemId) const;
  uint16_t getWallItemForAlign(WallAlign align) const;
  std::optional<uint16_t> findNextWallVariant(uint16_t currentItemId) const;
  std::optional<DoorNode> getDoorItemForAlign(WallAlign align, DoorType type,
                                              bool open,
                                              bool preferLocked) const;
  std::optional<DoorNode> findDoorForItem(uint16_t itemId) const;
  std::vector<const WallBrush *> getRedirectBrushes() const;
  BrushRegistry &getBrushRegistry() const { return registry_; }

protected:
  BrushRegistry &brushRegistry() const { return registry_; }

private:
  bool isWallGroupItem(uint16_t itemId) const;
  std::optional<WallAlign> findAlignmentForItem(uint16_t itemId) const;
  std::optional<WallAlign> findTileAlignment(const Domain::Tile &tile) const;
  uint16_t selectWallItem(WallAlign align) const;
  std::optional<DoorNode> selectDoorItem(WallAlign align, DoorType type,
                                         bool open, bool preferLocked) const;
  void rebuildTile(Domain::ChunkedMap &map, const Domain::Position &pos) const;
  void rebuildNeighbors(Domain::ChunkedMap &map,
                        const Domain::Position &center) const;
  bool connectsTo(const IBrush *brush) const;
  bool tileHasWallGroup(const Domain::Tile *tile) const;

  BrushRegistry &registry_;
  std::array<WallNode, 17> wallNodes_{};
  std::array<std::vector<DoorNode>, 17> doorNodes_{};
  std::unordered_set<uint16_t> ownedItemIds_;
  std::unordered_set<std::string> redirectNames_;
};

} // namespace MapEditor::Brushes
