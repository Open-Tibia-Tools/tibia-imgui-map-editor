#pragma once

#include "IPreviewProvider.h"

namespace MapEditor {
namespace Services {
class BrushSettingsService;
}
} // namespace MapEditor

namespace MapEditor::Services::Preview {

class MultiItemBrushPreviewProvider : public IPreviewProvider {
public:
  MultiItemBrushPreviewProvider(std::vector<PreviewTileData> prototypeTiles,
                                BrushSettingsService *brushSettings = nullptr,
                                bool repeatByBrushShape = true);

  bool isActive() const override { return !prototypeTiles_.empty(); }
  Domain::Position getAnchorPosition() const override { return anchor_; }
  const std::vector<PreviewTileData> &getTiles() const override;
  PreviewBounds getBounds() const override { return bounds_; }
  PreviewStyle getStyle() const override;
  void updateCursorPosition(const Domain::Position &cursor) override {
    anchor_ = cursor;
  }
  bool needsRegeneration() const override { return needsRegen_; }
  void regenerate() override;

private:
  bool checkSettingsChanged() const;
  void buildPreview();

  std::vector<PreviewTileData> prototypeTiles_;
  BrushSettingsService *brushSettings_ = nullptr;
  bool repeatByBrushShape_ = true;
  Domain::Position anchor_{0, 0, 0};
  std::vector<PreviewTileData> tiles_;
  PreviewBounds bounds_;
  mutable bool needsRegen_ = true;
  mutable std::vector<std::pair<int, int>> cachedOffsets_;
};

} // namespace MapEditor::Services::Preview
