#pragma once

#include "Brushes/Core/BrushBase.h"
#include "Brushes/Data/BorderBlock.h"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace MapEditor::Brushes {

class BrushRegistry;

class GroundBrush : public BrushBase {
public:
  struct SpecificCaseRule {
    std::vector<uint32_t> itemsToMatch;
    uint32_t toReplaceId = 0;
    uint32_t withId = 0;
    bool deleteAll = false;
    bool keepBorder = false;
  };

  struct BorderRule {
    BorderBlock block;
    bool outer = false;
    bool superBorder = false;
    std::string targetName;
    bool targetNone = false;
    std::vector<SpecificCaseRule> specificCases;
  };

  GroundBrush(std::string name, uint32_t lookId, BrushRegistry &registry);

  BrushType getType() const override { return BrushType::Ground; }
  bool needsBorderUpdate() const override { return true; }

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;
  bool ownsItem(const Domain::Item *item) const override;

  void addGroundItem(uint16_t itemId, uint32_t chance);
  void addFriend(const std::string &name);
  void addEnemy(const std::string &name);
  void addBorderRule(BorderRule rule);
  void setOptionalBorder(BorderBlock border, bool soloOptional);
  void setZOrder(int zOrder) { zOrder_ = zOrder; }

  int getZOrder() const { return zOrder_; }
  bool hasOptionalBorderRule() const { return optionalBorder_.has_value(); }
  bool usesSoloOptionalBorder() const { return soloOptionalBorder_; }

  uint16_t getPreviewItemId() const;
  void rebuildAround(Domain::ChunkedMap &map, const Domain::Position &center) const;
  void rebuildTile(Domain::ChunkedMap &map, const Domain::Position &pos) const;
  static void resetAltReplaceState();

private:
  const BorderRule *findRuleFor(const GroundBrush *other,
                                bool requireOuter) const;
  bool connectsTo(const GroundBrush *other) const;
  bool isFriendName(const std::string &name) const;
  uint16_t selectWeightedItem(
      const std::vector<std::pair<uint16_t, uint32_t>> &items) const;
  void updateBorderItems(Domain::ChunkedMap &map, Domain::Tile &tile) const;
  bool isBorderItem(uint16_t itemId) const;

  BrushRegistry &registry_;
  int zOrder_ = 0;
  std::vector<std::pair<uint16_t, uint32_t>> groundItems_;
  std::vector<BorderRule> borderRules_;
  std::optional<BorderBlock> optionalBorder_;
  bool soloOptionalBorder_ = false;
  std::unordered_set<uint16_t> ownedItemIds_;
  std::unordered_set<std::string> friendNames_;
  std::unordered_set<std::string> enemyNames_;
};

} // namespace MapEditor::Brushes
