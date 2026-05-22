#pragma once

#include "Brushes/Core/BrushBase.h"
#include "Brushes/Data/DoodadAlternative.h"
#include "Services/Preview/PreviewTypes.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace MapEditor::Brushes {

class BrushRegistry;

class DoodadBrush : public BrushBase {
public:
  using DoodadLayout = std::vector<Services::Preview::PreviewTileData>;

  DoodadBrush(std::string name, uint32_t lookId, BrushRegistry &registry,
              bool draggable);

  BrushType getType() const override { return BrushType::Doodad; }
  bool needsBorderUpdate() const override { return redoBorders_; }
  size_t getMaxVariation() const override { return alternatives_.size(); }
  void setVariation(size_t index) override { activeVariation_ = index; }

  void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
            const DrawContext &ctx) override;
  void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) override;
  bool ownsItem(const Domain::Item *item) const override;

  void addAlternative(DoodadAlternative alternative);
  void setOnBlocking(bool value) { onBlocking_ = value; }
  void setOnDuplicate(bool value) { onDuplicate_ = value; }
  void setRedoBorders(bool value) { redoBorders_ = value; }
  void setOneSize(bool value) { oneSize_ = value; }
  void setRemoveOptionalBorder(bool value) { removeOptionalBorder_ = value; }
  void setThickness(float value) { thickness_ = value; }
  [[nodiscard]] float getThickness() const { return thickness_; }
  [[nodiscard]] bool isOneSize() const { return oneSize_; }
  [[nodiscard]] bool removesOptionalBorder() const {
    return removeOptionalBorder_;
  }
  [[nodiscard]] size_t getVariation() const { return activeVariation_; }

  uint16_t getPreviewItemId() const;
  DoodadLayout buildPreviewTiles(const Domain::Position &anchor,
                                 const Services::BrushSettingsService *brushSettings) const;
  DoodadLayout buildPlacementLayout(const Domain::Position &center,
                                    const Services::BrushSettingsService *brushSettings,
                                    size_t preferredVariation,
                                    const Domain::ChunkedMap *map = nullptr,
                                    bool forcePlace = false) const;
  std::vector<Domain::Position>
  getPlacementPositions(const Domain::Position &center,
                       const Services::BrushSettingsService *brushSettings,
                       size_t preferredVariation,
                       const Domain::ChunkedMap *map = nullptr,
                       bool forcePlace = false) const;
  void applyPlacementLayout(Domain::ChunkedMap &map,
                            const Domain::Position &center,
                            const DoodadLayout &layout,
                            const DrawContext &ctx) const;

private:
  const DoodadAlternative *selectAlternative(size_t preferredIndex) const;
  bool tileHasOwnItem(const Domain::Tile *tile) const;

  BrushRegistry &registry_;
  std::vector<DoodadAlternative> alternatives_;
  std::unordered_set<uint16_t> ownedItemIds_;
  size_t activeVariation_ = 0;
  float thickness_ = 1.0f;
  bool onBlocking_ = false;
  bool onDuplicate_ = false;
  bool redoBorders_ = false;
  bool oneSize_ = false;
  bool removeOptionalBorder_ = false;
};

} // namespace MapEditor::Brushes
