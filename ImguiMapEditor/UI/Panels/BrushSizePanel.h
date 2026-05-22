#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace MapEditor {
namespace Brushes {
class BrushController;
class WaypointBrush;
}
namespace Services {
class BrushSettingsService;
} // namespace Services
} // namespace MapEditor

namespace MapEditor {
namespace UI {
namespace Panels {

/**
 * Dockable Tool Options window for brush controls.
 *
 * Keeps the existing custom brush editor available behind a collapsible
 * section, but the default visible UI now mirrors the RME-style tool options
 * layout: active brush shortcuts, brush-specific toggles, size/thickness, and
 * context-sensitive settings.
 */
class BrushSizePanel {
public:
  using SaveCallback = std::function<void()>;

  explicit BrushSizePanel(Services::BrushSettingsService *brushService,
                          Brushes::BrushController *brushController,
                          SaveCallback onSave = nullptr);
  ~BrushSizePanel() = default;

  // Non-copyable
  BrushSizePanel(const BrushSizePanel &) = delete;
  BrushSizePanel &operator=(const BrushSizePanel &) = delete;

  void render(bool *p_visible = nullptr);

private:
  Services::BrushSettingsService *service_;
  Brushes::BrushController *controller_;
  SaveCallback onSave_;
  bool symmetricSize_ = true;

  // Custom brush editing state
  bool isEditingCustomBrush_ = false;
  bool isNewBrushMode_ = false; // For pulsing "New" state
  std::string editingBrushName_;
  char waypointNameBuffer_[128]{};
  std::vector<std::vector<bool>> customGrid_; // 11×11 editable grid
  static constexpr int GRID_SIZE = 11;
  const Brushes::WaypointBrush *cachedWaypointBrush_ = nullptr;

  // Layout sections
  void renderToolbar();
  void renderBrushOptions();
  void renderTopRow();
  void renderSizeSliders();
  void renderCustomBrushControls();
  void renderPreviewSection(float availableHeight, bool isInteractive);
  void renderBottomButtons(); // New/Save/Clear/Delete buttons
  void renderPresetButtons();
  void renderSpawnSection(); // Spawn settings UI

  // Grid drawing (interactive or read-only)
  void drawInteractiveGrid(float maxSize);
  void drawReadOnlyGrid(float maxSize);

  // Custom brush management
  void loadSelectedBrushToGrid();
  void saveGridAsNewBrush();
  void saveGridToCurrentBrush();
  void deleteCurrentBrush();
  void applyPreset(const char *preset);
  void syncGridToService();

  // Persistence helpers
  void autoSaveBrushes();
};

} // namespace Panels
} // namespace UI
} // namespace MapEditor
