#pragma once

#include "IPreviewProvider.h"

namespace MapEditor::Brushes {
class DoodadBrush;
}

namespace MapEditor::Services {
class BrushSettingsService;
}

namespace MapEditor::Services::Preview {

class DoodadBrushPreviewProvider : public IPreviewProvider {
public:
  DoodadBrushPreviewProvider(const Brushes::DoodadBrush &brush,
                             BrushSettingsService *brushSettings = nullptr);

  bool isActive() const override;
  Domain::Position getAnchorPosition() const override;
  const std::vector<PreviewTileData> &getTiles() const override;
  PreviewBounds getBounds() const override;
  PreviewStyle getStyle() const override;
  void updateCursorPosition(const Domain::Position &cursor) override;
  bool needsRegeneration() const override { return needsRegen_; }
  void regenerate() override;

private:
  bool checkSettingsChanged() const;
  void buildPreview();

  const Brushes::DoodadBrush &brush_;
  BrushSettingsService *brushSettings_ = nullptr;
  Domain::Position anchor_{0, 0, 0};
  std::vector<PreviewTileData> tiles_;
  PreviewBounds bounds_;
  mutable bool needsRegen_ = true;
  mutable std::vector<std::pair<int, int>> cachedOffsets_;
};

} // namespace MapEditor::Services::Preview
