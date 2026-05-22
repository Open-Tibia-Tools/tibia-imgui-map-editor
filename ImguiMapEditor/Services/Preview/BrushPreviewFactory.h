#pragma once
#include <memory>

namespace MapEditor::Brushes {
class IBrush;
}

namespace MapEditor::Services {
class BrushSettingsService;
}

namespace MapEditor::Services::Preview {

class IPreviewProvider;

/**
 * Factory that creates preview providers based on brush type.
 *
 * Centralizes preview creation logic in one service.
 * Uses dynamic_cast to detect brush types and create appropriate providers.
 *
 * Supported brushes:
 * - RawBrush -> RawBrushPreviewProvider
 * - CreatureBrush -> (future) CreaturePreviewProvider
 * - Other types -> nullptr (no preview)
 */
class BrushPreviewFactory {
public:
  /**
   * Create appropriate preview provider for the given brush.
   * @param brush The brush to create a provider for
   * @param settings Brush settings for size/shape
   * @return Provider, or nullptr if brush has no preview support
   */
  std::unique_ptr<IPreviewProvider>
  createProvider(const ::MapEditor::Brushes::IBrush *brush,
                 BrushSettingsService *settings);
};

} // namespace MapEditor::Services::Preview
