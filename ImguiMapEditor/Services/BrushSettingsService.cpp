#include "BrushSettingsService.h"

#include <cstdlib>
#include <fstream>
#include <ranges>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor::Services {

namespace {

[[nodiscard]] int snapToDiscreteBrushSize(int size) {
  size = std::clamp(size, BrushSettingsService::MIN_SIZE,
                    BrushSettingsService::MAX_SIZE);

  auto best = BrushSettingsService::STANDARD_SIZE_PROGRESSION.front();
  auto bestDistance = std::abs(best - size);
  for (const auto candidate :
       BrushSettingsService::STANDARD_SIZE_PROGRESSION) {
    const auto distance = std::abs(candidate - size);
    if (distance < bestDistance) {
      best = candidate;
      bestDistance = distance;
    }
  }
  return best;
}

[[nodiscard]] int nextDiscreteBrushSize(int current) {
  current = snapToDiscreteBrushSize(current);
  for (const auto candidate :
       BrushSettingsService::STANDARD_SIZE_PROGRESSION) {
    if (candidate > current) {
      return candidate;
    }
  }
  return BrushSettingsService::STANDARD_SIZE_PROGRESSION.back();
}

[[nodiscard]] int previousDiscreteBrushSize(int current) {
  current = snapToDiscreteBrushSize(current);
  for (auto it = BrushSettingsService::STANDARD_SIZE_PROGRESSION.rbegin();
       it != BrushSettingsService::STANDARD_SIZE_PROGRESSION.rend(); ++it) {
    if (*it < current) {
      return *it;
    }
  }
  return BrushSettingsService::STANDARD_SIZE_PROGRESSION.front();
}

} // namespace

int BrushSettingsService::normalizeStandardSize(int size) {
  return snapToDiscreteBrushSize(size);
}

int BrushSettingsService::getStandardSizeProgressionIndexForValue(int size) {
  const auto normalized = normalizeStandardSize(size);
  const auto it =
      std::ranges::find(STANDARD_SIZE_PROGRESSION, normalized);
  if (it == STANDARD_SIZE_PROGRESSION.end()) {
    return 0;
  }
  return static_cast<int>(std::distance(STANDARD_SIZE_PROGRESSION.begin(), it));
}

int BrushSettingsService::getNextStandardSize() const {
  return nextDiscreteBrushSize(standardSize_);
}

int BrushSettingsService::getPreviousStandardSize() const {
  return previousDiscreteBrushSize(standardSize_);
}

// ========================
// Brush Type
// ========================

void BrushSettingsService::setBrushType(BrushType type) {
  if (type_ != type) {
    type_ = type;
    notifyChanged();
  }
}

// ========================
// Size Mode
// ========================

void BrushSettingsService::setBrushSizeMode(BrushSizeMode mode) {
  if (sizeMode_ != mode) {
    sizeMode_ = mode;
    notifyChanged();
  }
}

void BrushSettingsService::setPreviewBorder(bool enabled) {
  if (previewBorder_ != enabled) {
    previewBorder_ = enabled;
    notifyChanged();
  }
}

void BrushSettingsService::setLockDoors(bool enabled) {
  if (lockDoors_ != enabled) {
    lockDoors_ = enabled;
    notifyChanged();
  }
}

// ========================
// Standard Size (now direct tile count, not radius)
// ========================

void BrushSettingsService::setStandardSize(int size) {
  size = std::clamp(size, MIN_SIZE, MAX_SIZE);

  if (std::ranges::find(STANDARD_SIZE_PROGRESSION, size) ==
      STANDARD_SIZE_PROGRESSION.end()) {
    if (size > standardSize_) {
      size = getNextStandardSize();
    } else if (size < standardSize_) {
      size = getPreviousStandardSize();
    } else {
      size = normalizeStandardSize(size);
    }
  }

  if (standardSize_ != size) {
    standardSize_ = size;
    notifyChanged();
  }
}

void BrushSettingsService::increaseSize() {
  setStandardSize(getNextStandardSize());
}

void BrushSettingsService::decreaseSize() {
  setStandardSize(getPreviousStandardSize());
}

// ========================
// Custom Dimensions
// ========================

void BrushSettingsService::setCustomDimensions(int width, int height) {
  width = std::clamp(width, MIN_SIZE, MAX_SIZE);
  height = std::clamp(height, MIN_SIZE, MAX_SIZE);

  if (customWidth_ != width || customHeight_ != height) {
    customWidth_ = width;
    customHeight_ = height;
    notifyChanged();
  }
}

// ========================
// Computed Properties
// ========================

int BrushSettingsService::getEffectiveWidth() const {
  if (type_ == BrushType::Custom) {
    const auto *brush = getSelectedCustomBrush();
    if (brush && !brush->offsets.empty()) {
      int minX = 0, maxX = 0;
      for (const auto &[dx, dy] : brush->offsets) {
        minX = std::min(minX, dx);
        maxX = std::max(maxX, dx);
      }
      return maxX - minX + 1;
    }
    return 1;
  }

  if (sizeMode_ == BrushSizeMode::CustomDimensions) {
    return customWidth_;
  }

  // Standard size: direct tile count (size=3 means 3 tiles wide)
  return standardSize_;
}

int BrushSettingsService::getEffectiveHeight() const {
  if (type_ == BrushType::Custom) {
    const auto *brush = getSelectedCustomBrush();
    if (brush && !brush->offsets.empty()) {
      int minY = 0, maxY = 0;
      for (const auto &[dx, dy] : brush->offsets) {
        minY = std::min(minY, dy);
        maxY = std::max(maxY, dy);
      }
      return maxY - minY + 1;
    }
    return 1;
  }

  if (sizeMode_ == BrushSizeMode::CustomDimensions) {
    return customHeight_;
  }

  // Standard size: direct tile count (size=3 means 3 tiles tall)
  return standardSize_;
}

// ========================
// Custom Brushes
// ========================

void BrushSettingsService::addCustomBrush(const CustomBrushShape &brush) {
  // Check if brush with same name exists
  for (auto &existing : customBrushes_) {
    if (existing.name == brush.name) {
      existing = brush;
      existing.computeOffsets();
      notifyChanged();
      return;
    }
  }

  // Add new brush
  customBrushes_.push_back(brush);
  customBrushes_.back().computeOffsets();
  notifyChanged();
}

void BrushSettingsService::removeCustomBrush(const std::string &name) {
  auto it = std::remove_if(
      customBrushes_.begin(), customBrushes_.end(),
      [&name](const CustomBrushShape &b) { return b.name == name; });

  if (it != customBrushes_.end()) {
    customBrushes_.erase(it, customBrushes_.end());

    // Clear selection if removed brush was selected
    if (selectedCustomBrushName_ == name) {
      selectedCustomBrushName_.clear();
    }
    notifyChanged();
  }
}

void BrushSettingsService::selectCustomBrush(const std::string &name) {
  if (selectedCustomBrushName_ != name) {
    selectedCustomBrushName_ = name;
    notifyChanged();
  }
}

const CustomBrushShape *BrushSettingsService::getSelectedCustomBrush() const {
  if (selectedCustomBrushName_.empty()) {
    return nullptr;
  }

  for (const auto &brush : customBrushes_) {
    if (brush.name == selectedCustomBrushName_) {
      return &brush;
    }
  }
  return nullptr;
}

// ========================
// Core API: Position Calculation
// ========================

std::vector<Domain::Position>
BrushSettingsService::getBrushPositions(const Domain::Position &center) const {

  std::vector<Domain::Position> positions;
  auto offsets = getBrushOffsets();

  positions.reserve(offsets.size());
  for (const auto &[dx, dy] : offsets) {
    positions.emplace_back(center.x + dx, center.y + dy, center.z);
  }

  return positions;
}

std::vector<std::pair<int, int>> BrushSettingsService::getBrushOffsets() const {
  switch (type_) {
  case BrushType::Square:
    return calculateSquareOffsets();
  case BrushType::Circle:
    return calculateCircleOffsets();
  case BrushType::Custom:
    return calculateCustomOffsets();
  }
  return {{0, 0}}; // Fallback: single tile
}

// ========================
// Position Calculation Helpers
// ========================

std::vector<std::pair<int, int>>
BrushSettingsService::calculateSquareOffsets() const {
  std::vector<std::pair<int, int>> offsets;

  int width, height;
  if (sizeMode_ == BrushSizeMode::CustomDimensions) {
    width = customWidth_;
    height = customHeight_;
  } else {
    // Standard: direct tile count (size=3 means 3x3 grid)
    width = height = standardSize_;
  }

  // Calculate half extents (center is at 0,0)
  int halfW = width / 2;
  int halfH = height / 2;

  // For even sizes, bias toward negative
  int startX = -halfW;
  int endX = width - halfW - 1;
  int startY = -halfH;
  int endY = height - halfH - 1;

  for (int dy = startY; dy <= endY; ++dy) {
    for (int dx = startX; dx <= endX; ++dx) {
      offsets.emplace_back(dx, dy);
    }
  }

  return offsets;
}

std::vector<std::pair<int, int>>
BrushSettingsService::calculateCircleOffsets() const {
  std::vector<std::pair<int, int>> offsets;

  int width, height;
  if (sizeMode_ == BrushSizeMode::CustomDimensions) {
    width = customWidth_;
    height = customHeight_;
  } else {
    // Standard: direct tile count
    width = height = standardSize_;
  }

  if (width == 1 && height == 1) {
    // Size 1 = single tile
    offsets.emplace_back(0, 0);
    return offsets;
  }

  // Calculate half extents for ellipse
  int halfW = width / 2;
  int halfH = height / 2;

  float rX = static_cast<float>(width) / 2.0f;
  float rY = static_cast<float>(height) / 2.0f;

  int startX = -halfW;
  int endX = width - halfW - 1;
  int startY = -halfH;
  int endY = height - halfH - 1;

  for (int dy = startY; dy <= endY; ++dy) {
    for (int dx = startX; dx <= endX; ++dx) {
      // Check if point is inside ellipse (with 0.5 offset for center of tile)
      float nx = (static_cast<float>(dx) + 0.5f) / rX;
      float ny = (static_cast<float>(dy) + 0.5f) / rY;

      if (nx * nx + ny * ny <= 1.0f) {
        offsets.emplace_back(dx, dy);
      }
    }
  }

  // Ensure at least center tile
  if (offsets.empty()) {
    offsets.emplace_back(0, 0);
  }

  return offsets;
}

std::vector<std::pair<int, int>>
BrushSettingsService::calculateCustomOffsets() const {
  const auto *brush = getSelectedCustomBrush();
  if (!brush || brush->offsets.empty()) {
    // No custom brush selected, return single tile
    return {{0, 0}};
  }

  return brush->offsets;
}

// ========================
// Persistence
// ========================

bool BrushSettingsService::saveCustomBrushes(
    const std::string &filepath) const {
  try {
    nlohmann::json j;
    j["version"] = 1;

    nlohmann::json brushesArray = nlohmann::json::array();
    for (const auto &brush : customBrushes_) {
      nlohmann::json brushObj;
      brushObj["name"] = brush.name;
      brushObj["gridSize"] = brush.gridSize;

      // Flatten grid to 1D array for compact storage
      nlohmann::json gridData = nlohmann::json::array();
      for (const auto &row : brush.grid) {
        for (bool cell : row) {
          gridData.push_back(cell ? 1 : 0);
        }
      }
      brushObj["grid"] = gridData;

      brushesArray.push_back(brushObj);
    }
    j["brushes"] = brushesArray;

    std::ofstream file(filepath);
    if (!file.is_open()) {
      spdlog::error("Failed to open file for writing: {}", filepath);
      return false;
    }

    file << j.dump(2);
    spdlog::info("Saved {} custom brushes to {}", customBrushes_.size(),
                 filepath);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Failed to save custom brushes: {}", e.what());
    return false;
  }
}

bool BrushSettingsService::loadCustomBrushes(const std::string &filepath) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      // File doesn't exist yet - that's okay
      spdlog::debug("Custom brushes file not found: {}", filepath);
      return true;
    }

    nlohmann::json j;
    file >> j;

    int version = j.value("version", 1);
    if (version != 1) {
      spdlog::warn("Unknown custom brushes file version: {}", version);
    }

    customBrushes_.clear();

    for (const auto &brushObj : j["brushes"]) {
      CustomBrushShape brush;
      brush.name = brushObj["name"].get<std::string>();
      brush.gridSize = brushObj.value("gridSize", DEFAULT_CUSTOM_GRID_SIZE);

      // Reconstruct grid from 1D array
      brush.grid.resize(brush.gridSize,
                        std::vector<bool>(brush.gridSize, false));

      const auto &gridData = brushObj["grid"];
      size_t idx = 0;
      for (int y = 0; y < brush.gridSize && idx < gridData.size(); ++y) {
        for (int x = 0; x < brush.gridSize && idx < gridData.size();
             ++x, ++idx) {
          brush.grid[y][x] = gridData[idx].get<int>() != 0;
        }
      }

      brush.computeOffsets();
      customBrushes_.push_back(std::move(brush));
    }

    spdlog::info("Loaded {} custom brushes from {}", customBrushes_.size(),
                 filepath);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Failed to load custom brushes: {}", e.what());
    return false;
  }
}

// ========================
// Change Notification
// ========================

void BrushSettingsService::notifyChanged() {
  if (onSettingsChanged_) {
    onSettingsChanged_();
  }
}

} // namespace MapEditor::Services
