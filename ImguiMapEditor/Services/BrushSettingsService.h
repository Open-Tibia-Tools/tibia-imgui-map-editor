#pragma once

#include "Domain/Position.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace MapEditor::Services {

/**
 * Brush shape type - determines how brush positions are calculated.
 */
enum class BrushType {
  Square, // Rectangular brush pattern
  Circle, // Circular brush pattern
  Custom  // User-defined custom shape
};

/**
 * Brush size mode - determines how dimensions are specified.
 */
enum class BrushSizeMode {
  Standard,        // Symmetric radius (produces NxN square or N-radius circle)
  CustomDimensions // Independent width and height
};

/**
 * Custom brush shape definition.
 * Contains a 2D grid of selected tiles and pre-computed offsets.
 */
struct CustomBrushShape {
  std::string name;
  std::vector<std::vector<bool>> grid; // [row][col], true = selected
  int gridSize = 10;                   // Default 10x10 grid

  // Pre-computed offsets relative to center (calculated on creation/load)
  std::vector<std::pair<int, int>> offsets;

  CustomBrushShape() = default;
  CustomBrushShape(const std::string &n, int size = 10)
      : name(n), gridSize(size) {
    grid.resize(size, std::vector<bool>(size, false));
  }

  /**
   * Compute offsets from grid data.
   * Center is at (gridSize/2, gridSize/2).
   */
  void computeOffsets() {
    offsets.clear();
    int center = gridSize / 2;
    for (int y = 0; y < gridSize; ++y) {
      for (int x = 0; x < gridSize; ++x) {
        if (grid[y][x]) {
          offsets.emplace_back(x - center, y - center);
        }
      }
    }
  }

  bool isEmpty() const {
    for (const auto &row : grid) {
      for (bool cell : row) {
        if (cell)
          return false;
      }
    }
    return true;
  }
};

/**
 * Central service for brush size, shape, and custom brush management.
 *
 * This service holds all brush settings state and provides:
 * - Brush type selection (Square/Circle/Custom)
 * - Size mode (Standard symmetric or Custom W×H)
 * - Standard size control (radius 1-10)
 * - Custom dimensions (independent width/height)
 * - Custom brush creation, storage, and selection
 * - Position calculation for painting operations
 * - JSON persistence for custom brushes
 *
 * Usage:
 *   // Set brush type and size
 *   service.setBrushType(BrushType::Square);
 *   service.setStandardSize(3);
 *
 *   // Get positions to paint
 *   auto positions = service.getBrushPositions(cursorPos);
 *   for (const auto& pos : positions) {
 *       paintTile(pos);
 *   }
 */
class BrushSettingsService {
public:
  static constexpr int MIN_SIZE = 1;
  static constexpr int MAX_SIZE = 11;
  static constexpr int DEFAULT_CUSTOM_GRID_SIZE = 10;
  static constexpr std::array<int, 6> STANDARD_SIZE_PROGRESSION{1, 2, 4, 6, 8,
                                                                11};

  BrushSettingsService() = default;
  ~BrushSettingsService() = default;

  // Non-copyable
  BrushSettingsService(const BrushSettingsService &) = delete;
  BrushSettingsService &operator=(const BrushSettingsService &) = delete;

  // ========================
  // Brush Type
  // ========================

  void setBrushType(BrushType type);
  BrushType getBrushType() const { return type_; }

  /**
   * Check if size mode controls are enabled.
   * Returns false when Custom brush type is selected.
   */
  bool isSizeModeEnabled() const { return type_ != BrushType::Custom; }

  // ========================
  // Size Mode
  // ========================

  void setBrushSizeMode(BrushSizeMode mode);
  BrushSizeMode getBrushSizeMode() const { return sizeMode_; }

  // ========================
  // Tool Options
  // ========================

  void setPreviewBorder(bool enabled);
  bool getPreviewBorder() const { return previewBorder_; }

  void setLockDoors(bool enabled);
  bool getLockDoors() const { return lockDoors_; }

  // ========================
  // Standard Size
  // ========================

  void setStandardSize(int size);
  int getStandardSize() const { return standardSize_; }

  void increaseSize();
  void decreaseSize();

  [[nodiscard]] static std::span<const int> getStandardSizeProgression() {
    return STANDARD_SIZE_PROGRESSION;
  }
  [[nodiscard]] static int normalizeStandardSize(int size);
  [[nodiscard]] static int
  getStandardSizeProgressionIndexForValue(int size);
  [[nodiscard]] int getStandardSizeProgressionIndex() const {
    return getStandardSizeProgressionIndexForValue(standardSize_);
  }
  [[nodiscard]] int getNextStandardSize() const;
  [[nodiscard]] int getPreviousStandardSize() const;

  // ========================
  // Custom Dimensions (independent W×H)
  // ========================

  void setCustomDimensions(int width, int height);
  int getCustomWidth() const { return customWidth_; }
  int getCustomHeight() const { return customHeight_; }

  // ========================
  // Computed Properties
  // ========================

  /**
   * Get effective brush width based on current settings.
   */
  int getEffectiveWidth() const;

  /**
   * Get effective brush height based on current settings.
   */
  int getEffectiveHeight() const;

  // ========================
  // Custom Brushes
  // ========================

  /**
   * Add or update a custom brush.
   */
  void addCustomBrush(const CustomBrushShape &brush);

  /**
   * Remove a custom brush by name.
   */
  void removeCustomBrush(const std::string &name);

  /**
   * Select a custom brush by name.
   * Only effective when BrushType is Custom.
   */
  void selectCustomBrush(const std::string &name);

  /**
   * Get currently selected custom brush (may be nullptr).
   */
  const CustomBrushShape *getSelectedCustomBrush() const;

  /**
   * Get all custom brushes.
   */
  const std::vector<CustomBrushShape> &getCustomBrushes() const {
    return customBrushes_;
  }

  // ========================
  // Core API: Position Calculation
  // ========================

  /**
   * Get all tile positions that should be affected by the brush.
   *
   * @param center The center position (usually cursor position)
   * @return Vector of positions to paint/erase
   */
  std::vector<Domain::Position>
  getBrushPositions(const Domain::Position &center) const;

  /**
   * Get relative offsets for current brush settings.
   * These are (dx, dy) pairs relative to center (0,0).
   * Useful for preview rendering.
   */
  std::vector<std::pair<int, int>> getBrushOffsets() const;

  // ========================
  // Persistence
  // ========================

  /**
   * Save custom brushes to JSON file.
   */
  bool saveCustomBrushes(const std::string &filepath) const;

  /**
   * Load custom brushes from JSON file.
   */
  bool loadCustomBrushes(const std::string &filepath);

  // ========================
  // Change Notification
  // ========================

  using OnSettingsChangedCallback = std::function<void()>;

  /**
   * Set callback to be invoked when any setting changes.
   * Used to update preview when brush parameters change.
   */
  void setOnSettingsChanged(OnSettingsChangedCallback callback) {
    onSettingsChanged_ = std::move(callback);
  }

  // ========================
  // Spawn Settings (for CreatureBrush auto-spawn)
  // ========================

  /**
   * Enable/disable automatic spawn creation when placing creatures.
   */
  void setAutoCreateSpawn(bool enabled) {
    autoCreateSpawn_ = enabled;
    notifyChanged();
  }
  bool getAutoCreateSpawn() const { return autoCreateSpawn_; }

  /**
   * Default spawn radius (1-10 tiles).
   */
  void setDefaultSpawnRadius(int radius) {
    defaultSpawnRadius_ = std::clamp(radius, 1, 10);
    notifyChanged();
  }
  int getDefaultSpawnRadius() const { return defaultSpawnRadius_; }

  /**
   * Default spawn timer in seconds.
   */
  void setDefaultSpawnTime(int seconds) {
    defaultSpawnTime_ = std::clamp(seconds, 1, 86400); // 1 sec to 24 hours
    notifyChanged();
  }
  int getDefaultSpawnTime() const { return defaultSpawnTime_; }

private:
  BrushType type_ = BrushType::Square;
  BrushSizeMode sizeMode_ = BrushSizeMode::Standard;
  int standardSize_ = 1;
  int customWidth_ = 1;
  int customHeight_ = 1;

  std::vector<CustomBrushShape> customBrushes_;
  std::string selectedCustomBrushName_;

  OnSettingsChangedCallback onSettingsChanged_;

  // Spawn settings
  bool autoCreateSpawn_ = false;
  int defaultSpawnRadius_ = 3;
  int defaultSpawnTime_ = 60; // seconds

  // Tool options
  bool previewBorder_ = true;
  bool lockDoors_ = false;

  void notifyChanged();

  // Position calculation helpers
  std::vector<std::pair<int, int>> calculateSquareOffsets() const;
  std::vector<std::pair<int, int>> calculateCircleOffsets() const;
  std::vector<std::pair<int, int>> calculateCustomOffsets() const;
};

} // namespace MapEditor::Services
