#pragma once
#include "Brushes/Core/IBrush.h"
#include "Brushes/Types/EraserBrush.h"
#include "Brushes/Types/FlagBrush.h"
#include "Brushes/Types/HouseBrush.h"
#include "Brushes/Types/SpawnBrush.h"
#include "Brushes/Types/WaypointBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/Position.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace MapEditor::Brushes {
class BrushRegistry;
}

namespace MapEditor::Services {
class ClientDataService;
class BrushSettingsService;
namespace Preview {
class PreviewService;
class BrushPreviewFactory;
} // namespace Preview
} // namespace MapEditor::Services

namespace MapEditor::Brushes {

// Callback type for notifying when brush becomes active (to clear selection)
using OnBrushActivatedCallback = std::function<void()>;

/**
 * Controls brush selection and application.
 *
 * Supports all brush types via IBrush interface.
 * Uses BrushPreviewFactory to create preview providers.
 */
class BrushController {
public:
  BrushController() = default;
  ~BrushController() = default;

  // Non-copyable
  BrushController(const BrushController &) = delete;
  BrushController &operator=(const BrushController &) = delete;

  /**
   * Initialize with required dependencies.
   * @param map The map to operate on
   * @param historyManager The history manager for undo/redo
   * @param clientData Client data service for item type lookup
   */
  void initialize(Domain::ChunkedMap *map,
                  Domain::History::HistoryManager *historyManager,
                  Services::ClientDataService *clientData);

  /**
   * Set the current brush.
   * Uses BrushPreviewFactory to create appropriate preview provider.
   * @param brush The brush to activate (non-owning pointer)
   */
  void setBrush(IBrush *brush);

  /**
   * Set callback for when brush is activated (to clear selection).
   */
  void setOnBrushActivatedCallback(OnBrushActivatedCallback callback) {
    onBrushActivated_ = std::move(callback);
  }

  /**
   * Clear the current brush selection.
   */
  void clearBrush();

  /**
   * Set preview service for brush preview updates.
   */
  void setPreviewService(Services::Preview::PreviewService *previewService) {
    previewService_ = previewService;
  }

  /**
   * Set preview factory for creating preview providers.
   */
  void setPreviewFactory(Services::Preview::BrushPreviewFactory *factory) {
    previewFactory_ = factory;
  }

  /**
   * Check if a brush is currently selected.
   */
  bool hasBrush() const { return currentBrush_ != nullptr; }

  /**
   * Get the current brush (for queries).
   */
  const IBrush *getCurrentBrush() const { return currentBrush_; }

  /**
   * Get the current brush item ID (for RAW brush compatibility).
   * Returns nullopt if not in RAW mode.
   */
  std::optional<uint32_t> getCurrentItemId() const;

  /**
   * Apply the current brush at a position.
   * Uses IBrush::draw() internally.
   * @param pos Position to apply brush
   * @return true if brush was applied
   */
  bool applyBrush(const Domain::Position &pos);

  /**
   * Erase at a position using current brush.
   * Uses IBrush::undraw() internally.
   * @param pos Position to erase
   * @return true if erase was performed
   */
  bool eraseBrush(const Domain::Position &pos);

  /**
   * Start a new brush stroke (for drag operations).
   * Call endStroke() when drag completes.
   */
  void beginStroke();

  /**
   * Add a position to the current stroke.
   */
  void continueStroke(const Domain::Position &pos);

  /**
   * End the current stroke and push batch action.
   */
  void endStroke();

  /**
   * Check if currently in a stroke.
   */
  bool isInStroke() const { return strokeActive_; }

  // Brush Size Control
  static constexpr int MIN_BRUSH_SIZE = 1;
  static constexpr int MAX_BRUSH_SIZE = 10;

  int getBrushSize() const { return brushSize_; }
  void setBrushSize(int size) {
    brushSize_ = std::clamp(size, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE);
  }

  /**
   * Set the brush settings service for size/shape calculations.
   */
  void setBrushSettingsService(Services::BrushSettingsService *service) {
    brushSettingsService_ = service;
    // Wire settings service to spawn brush
    spawnBrush_.setSettingsService(service);
  }

  /**
   * Get the brush settings service.
   */
  Services::BrushSettingsService *getBrushSettingsService() const {
    return brushSettingsService_;
  }

  /**
   * Activate the spawn brush for placing spawn points.
   */
  void activateSpawnBrush();

  /**
   * Get the spawn brush instance.
   */
  SpawnBrush *getSpawnBrush() { return &spawnBrush_; }

  /**
   * Activate a flag brush for zone painting.
   */
  void activatePZBrush() { setBrush(&pzBrush_); }
  void activateNoPvpBrush() { setBrush(&noPvpBrush_); }
  void activateNoLogoutBrush() { setBrush(&noLogoutBrush_); }
  void activatePvpZoneBrush() { setBrush(&pvpZoneBrush_); }

  FlagBrush *getPZBrush() { return &pzBrush_; }
  FlagBrush *getNoPvpBrush() { return &noPvpBrush_; }
  FlagBrush *getNoLogoutBrush() { return &noLogoutBrush_; }
  FlagBrush *getPvpZoneBrush() { return &pvpZoneBrush_; }

  void activateEraserBrush() { setBrush(&eraserBrush_); }
  EraserBrush *getEraserBrush() { return &eraserBrush_; }

  void activateHouseBrush() { setBrush(&houseBrush_); }
  HouseBrush *getHouseBrush() { return &houseBrush_; }

  void activateWaypointBrush() { setBrush(&waypointBrush_); }
  WaypointBrush *getWaypointBrush() { return &waypointBrush_; }

private:
  Domain::ChunkedMap *map_ = nullptr;
  Domain::History::HistoryManager *historyManager_ = nullptr;
  Services::ClientDataService *clientData_ = nullptr;

  // Unified brush pointer
  IBrush *currentBrush_ = nullptr;
  std::string currentBrushName_;

  // Callback when brush is activated
  OnBrushActivatedCallback onBrushActivated_;

  // Preview service for brush preview updates
  Services::Preview::PreviewService *previewService_ = nullptr;

  // Preview factory for creating preview providers
  Services::Preview::BrushPreviewFactory *previewFactory_ = nullptr;

  // Brush size (radius) - fallback if no BrushSettingsService
  int brushSize_ = 1;

  // Brush settings service for size/shape calculations
  Services::BrushSettingsService *brushSettingsService_ = nullptr;

  // Owned spawn brush instance
  SpawnBrush spawnBrush_;

  // Flag brush instances (one per flag type)
  FlagBrush pzBrush_{Domain::TileFlag::ProtectionZone, "PZ"};
  FlagBrush noPvpBrush_{Domain::TileFlag::NoPvp, "NoPvP"};
  FlagBrush noLogoutBrush_{Domain::TileFlag::NoLogout, "NoLogout"};
  FlagBrush pvpZoneBrush_{Domain::TileFlag::PvpZone, "PvPZone"};

  // Eraser brush instance
  EraserBrush eraserBrush_;

  // House brush instance
  HouseBrush houseBrush_;

  // Waypoint brush instance
  WaypointBrush waypointBrush_;

  // Simple flag for stroke tracking (HistoryManager handles actual undo)
  bool strokeActive_ = false;

  // Track positions painted in current stroke (to avoid duplicates)
  struct PositionHash {
    size_t operator()(const std::tuple<int32_t, int32_t, int16_t> &p) const {
      return std::hash<int64_t>()(static_cast<int64_t>(std::get<0>(p)) |
                                  (static_cast<int64_t>(std::get<1>(p)) << 20) |
                                  (static_cast<int64_t>(std::get<2>(p)) << 40));
    }
  };
  std::unordered_set<std::tuple<int32_t, int32_t, int16_t>, PositionHash>
      paintedPositions_;

  // Track last position within current stroke for line interpolation
  std::optional<Domain::Position> lastStrokePos_;

  // Helper: get all positions on line between two points (Bresenham)
  static std::vector<Domain::Position>
  getLinePositions(const Domain::Position &from,
                   const Domain::Position &to) ;

  // Paint tile using current brush
  void paintTileDirect(const Domain::Position &pos);
};

} // namespace MapEditor::Brushes
