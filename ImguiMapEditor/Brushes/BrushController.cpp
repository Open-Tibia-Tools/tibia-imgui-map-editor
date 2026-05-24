#include "BrushController.h"
#include "Domain/Item.h"
#include "Services/BrushSettingsService.h"
#include "Services/ClientDataService.h"
#include "Services/Preview/BrushPreviewFactory.h"
#include "Services/Preview/PreviewService.h"
#include "Types/RawBrush.h"
#include <cmath>
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

void BrushController::initialize(
    Domain::ChunkedMap *map, Domain::History::HistoryManager *historyManager,
    Services::ClientDataService *clientData) {
  map_ = map;
  historyManager_ = historyManager;
  clientData_ = clientData;
  spdlog::debug("[BrushController] Initialized with map, history manager, and "
                "client data");
}

void BrushController::setBrush(IBrush *brush) {
  if (!brush) {
    clearBrush();
    return;
  }

  currentBrush_ = brush;
  currentBrushName_ = brush->getName();

  // Use factory to create preview provider
  if (previewService_ && previewFactory_) {
    auto provider =
        previewFactory_->createProvider(brush, brushSettingsService_);
    if (provider) {
      previewService_->setProvider(std::move(provider));
    } else {
      previewService_->clearPreview();
    }
  } else if (previewService_) {
    // No factory available - clear preview
    previewService_->clearPreview();
    spdlog::warn("[BrushController] No preview factory available");
  }

  if (onBrushActivated_) {
    onBrushActivated_();
  }

  spdlog::info("[BrushController] Set brush: {}", brush->getName());
}

void BrushController::clearBrush() {
  currentBrush_ = nullptr;
  currentBrushName_.clear();

  // Clear preview
  if (previewService_) {
    previewService_->clearPreview();
  }

  spdlog::debug("[BrushController] Brush cleared");
}

void BrushController::activateSpawnBrush() {
  setBrush(&spawnBrush_);
  spdlog::info("[BrushController] Spawn brush activated");
}

std::optional<uint32_t> BrushController::getCurrentItemId() const {
  // Check if current brush is a RawBrush
  if (auto *rawBrush = dynamic_cast<RawBrush *>(currentBrush_)) {
    return rawBrush->getItemId();
  }
  return std::nullopt;
}

bool BrushController::applyBrush(const Domain::Position &pos) {
  if (!map_ || !historyManager_ || !currentBrush_) {
    return false;
  }

  // If in stroke mode, use optimized direct painting
  if (strokeActive_) {
    auto key = std::make_tuple(pos.x, pos.y, pos.z);
    if (paintedPositions_.count(key) > 0) {
      return true; // Already painted
    }
    paintedPositions_.insert(key);

    // Capture BEFORE state for undo
    const Domain::Tile *tile = map_->getTile(pos);
    historyManager_->recordTileBefore(pos, tile);

    paintTileDirect(pos);
    return true;
  }

  // Single click mode - use HistoryManager for undo support
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation("Brush: " + currentBrushName_,
                                  Domain::History::ActionType::Draw, nullptr);

  const Domain::Tile *tile = map_->getTile(pos);
  historyManager_->recordTileBefore(pos, tile);

  paintTileDirect(pos);

  historyManager_->endOperation(map_, nullptr);

  return true;
}

bool BrushController::eraseBrush(const Domain::Position &pos) {
  if (!map_ || !historyManager_ || !currentBrush_) {
    return false;
  }

  Domain::Tile *tile = map_->getTile(pos);
  if (!tile) {
    return false;
  }

  // Use HistoryManager for undo support
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation("Erase: " + currentBrushName_,
                                  Domain::History::ActionType::Delete, nullptr);
  historyManager_->recordTileBefore(pos, tile);

  // Use IBrush::undraw() for unified erasing
  currentBrush_->undraw(*map_, tile);

  historyManager_->endOperation(map_, nullptr);

  return true;
}

void BrushController::beginStroke() {
  if (!historyManager_ || !currentBrush_)
    return;

  // Start a new history operation for this stroke
  // Note: Selection service is nullptr since brushes don't affect selection
  historyManager_->beginOperation("Brush: " + currentBrushName_,
                                  Domain::History::ActionType::Draw, nullptr);

  // Set stroke active flag (HistoryManager handles actual undo)
  strokeActive_ = true;
  paintedPositions_.clear();
  lastStrokePos_.reset();
  spdlog::debug("[BrushController] Started brush stroke");
}

// Paint tile using IBrush::draw()
void BrushController::paintTileDirect(const Domain::Position &pos) {
  if (!map_ || !currentBrush_)
    return;

  Domain::Tile *tile = map_->getOrCreateTile(pos);
  if (!tile)
    return;

  // Create draw context with brush settings
  DrawContext ctx;
  ctx.variation = 0;
  ctx.isDragging = strokeActive_;
  ctx.brushSettings = brushSettingsService_;

  // Use unified IBrush::draw()
  currentBrush_->draw(*map_, tile, ctx);
}

void BrushController::continueStroke(const Domain::Position &pos) {
  if (!strokeActive_ || !historyManager_ || !currentBrush_)
    return;

  // Helper function to paint at a single position with duplicate tracking
  auto paintAtPosition = [&](const Domain::Position &targetPos) {
    auto key = std::make_tuple(targetPos.x, targetPos.y, targetPos.z);
    if (paintedPositions_.count(key) > 0) {
      return; // Already painted
    }
    paintedPositions_.insert(key);

    // Capture BEFORE state
    const Domain::Tile *tile = map_->getTile(targetPos);
    historyManager_->recordTileBefore(targetPos, tile);

    paintTileDirect(targetPos);
  };

  // Get all positions to paint based on brush settings
  std::vector<Domain::Position> brushPositions;
  if (brushSettingsService_) {
    brushPositions = brushSettingsService_->getBrushPositions(pos);
  } else {
    // Fallback: just paint the single position
    brushPositions.push_back(pos);
  }

  // First position of stroke
  if (!lastStrokePos_.has_value()) {
    for (const auto &brushPos : brushPositions) {
      paintAtPosition(brushPos);
    }
    lastStrokePos_ = pos;
    return;
  }

  // Interpolate line between last pos and current
  auto linePositions = getLinePositions(lastStrokePos_.value(), pos);
  lastStrokePos_ = pos;

  for (const auto &linePos : linePositions) {
    // For each line position, get all brush positions
    std::vector<Domain::Position> expandedPositions;
    if (brushSettingsService_) {
      expandedPositions = brushSettingsService_->getBrushPositions(linePos);
    } else {
      expandedPositions.push_back(linePos);
    }

    for (const auto &brushPos : expandedPositions) {
      paintAtPosition(brushPos);
    }
  }
}

void BrushController::endStroke() {
  if (!strokeActive_ || !historyManager_) {
    strokeActive_ = false;
    paintedPositions_.clear();
    lastStrokePos_.reset();
    return;
  }

  if (!paintedPositions_.empty()) {
    spdlog::debug("[BrushController] Ended stroke with {} tiles",
                  paintedPositions_.size());

    // End the history operation - captures AFTER states and pushes to history
    historyManager_->endOperation(map_, nullptr);
  } else {
    // No tiles painted - cancel the operation
    historyManager_->cancelOperation();
  }

  strokeActive_ = false;
  paintedPositions_.clear();
  lastStrokePos_.reset();
}

// Bresenham's line algorithm implementation
std::vector<Domain::Position>
BrushController::getLinePositions(const Domain::Position &from,
                                  const Domain::Position &to) {

  std::vector<Domain::Position> positions;

  int32_t x0 = from.x, y0 = from.y;
  int32_t x1 = to.x, y1 = to.y;
  int16_t const z = from.z; // Stay on same floor

  int32_t const dx = std::abs(x1 - x0);
  int32_t const dy = -std::abs(y1 - y0);
  int32_t const sx = x0 < x1 ? 1 : -1;
  int32_t const sy = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;

  while (true) {
    positions.push_back({x0, y0, z});

    if (x0 == x1 && y0 == y1)
      break;

    int32_t const e2 = 2 * err;
    if (e2 >= dy) {
      if (x0 == x1)
        break;
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      if (y0 == y1)
        break;
      err += dx;
      y0 += sy;
    }
  }

  return positions;
}

} // namespace MapEditor::Brushes
