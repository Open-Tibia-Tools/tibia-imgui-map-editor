#include "MapInputController.h"
#include "Application/EditorSession.h"
#include "Application/Selection/FloorScopeHelper.h"
#include "Application/Selection/PixelPerfectSelectionStrategy.h"
#include "Application/Selection/SmartSelectionStrategy.h"
#include "Brushes/BrushController.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/Item.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Domain/Tile.h"
#include "Services/Map/MapEditingService.hpp"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace MapEditor::AppLogic {

using namespace Domain::Selection;

MapInputController::MapInputController(Domain::SelectionSettings &settings,
                                       Services::ClientDataService *client_data)
    : settings_(settings), client_data_(client_data),
      last_was_pixel_perfect_(settings.use_pixel_perfect) {
  // Select strategy based on settings
  if (settings_.use_pixel_perfect && client_data_) {
    current_strategy_ =
        std::make_unique<PixelPerfectSelectionStrategy>(client_data_);
  } else {
    current_strategy_ = std::make_unique<SmartSelectionStrategy>();
  }
}

MapInputController::~MapInputController() = default;

void MapInputController::ensureCorrectStrategy() {
  // Check if strategy needs to be switched
  if (settings_.use_pixel_perfect != last_was_pixel_perfect_) {
    if (settings_.use_pixel_perfect && client_data_) {
      current_strategy_ =
          std::make_unique<PixelPerfectSelectionStrategy>(client_data_);
    } else {
      current_strategy_ = std::make_unique<SmartSelectionStrategy>();
    }
    last_was_pixel_perfect_ = settings_.use_pixel_perfect;
  }
}

void MapInputController::setClientDataService(
    Services::ClientDataService *client_data) {
  client_data_ = client_data;
  // Recreate strategy with updated client data
  if (settings_.use_pixel_perfect && client_data_) {
    current_strategy_ =
        std::make_unique<PixelPerfectSelectionStrategy>(client_data_);
  } else {
    current_strategy_ = std::make_unique<SmartSelectionStrategy>();
  }
  last_was_pixel_perfect_ = settings_.use_pixel_perfect;
}

void MapInputController::onLeftClick(const Domain::Position &pos, int mods,
                                     const glm::vec2 &pixel_offset,
                                     EditorSession *session) {
  if (!session)
    return;

  // BRUSH MODE: Paint single tile on click (atomic undo entry)
  if (brush_controller_ && brush_controller_->hasBrush() &&
      !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT))) {
    brush_controller_->applyBrush(pos);
    session->setModified(true);
    return;
  }

  auto &selection_service = session->getSelectionService();
  auto *map = session->getMap();

  // Ctrl+Shift+Click: TOGGLE ENTIRE TILE
  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT)) {
    if (map) {
      // If tile has any selection, remove all at position
      // Otherwise, add all entities at position
      if (selection_service.hasSelectionAt(pos)) {
        selection_service.removeAllAt(pos);
        spdlog::debug("[INPUT] Ctrl+Shift+Click - Deselected tile at ({},{})",
                      pos.x, pos.y);
      } else {
        selection_service.selectTile(map, pos);
        spdlog::debug("[INPUT] Ctrl+Shift+Click - Selected tile at ({},{})",
                      pos.x, pos.y);
      }
    }
    return;
  }

  // FIX: Shift+Click: Select ALL Items on Tile - CLEAR selection first
  if (mods & GLFW_MOD_SHIFT) {
    if (map) {
      selection_service.clear();
      selection_service.selectTile(map, pos);
      spdlog::debug("[INPUT] Shift+Click - Selected tile at ({}, {})", pos.x,
                    pos.y);
    }
    return;
  }

  // Smart Selection (Top Entity)
  ensureCorrectStrategy();
  auto entry = current_strategy_->selectAt(map, pos, pixel_offset);

  if (mods & GLFW_MOD_CONTROL) {
    // Ctrl+Click: TOGGLE Specific Entity
    selection_service.toggleEntity(map, entry);
    spdlog::debug("[INPUT] Ctrl+Click - Toggled entity at ({}, {}, {})", pos.x,
                  pos.y, pos.z);
  } else {
    // Regular click: Clear and Select Top Entity
    selection_service.clear();
    selection_service.addEntity(entry);
    spdlog::debug("[INPUT] Click - Selected entity at ({}, {}, {})", pos.x,
                  pos.y, pos.z);
  }
}

bool MapInputController::isSomethingSelectedAt(const Domain::Position &pos,
                                               const glm::vec2 &pixel_offset,
                                               EditorSession *session) {
  if (!session || !current_strategy_)
    return false;
  auto *map = session->getMap();
  ensureCorrectStrategy();
  auto entry = current_strategy_->selectAt(map, pos, pixel_offset);
  const auto &selection_service = session->getSelectionService();

  // Check if this specific entity is selected
  if (selection_service.isSelected(entry.id)) {
    return true;
  }

  // Also check if anything at this position is selected
  return selection_service.hasSelectionAt(pos);
}

void MapInputController::onLeftDragStart(const Domain::Position &pos,
                                         EditorSession *session) {
  if (!session)
    return;

  // BRUSH MODE: Start stroke
  if (brush_controller_ && brush_controller_->hasBrush()) {
    is_brush_dragging_ = true;
    last_brush_pos_ = pos;
    brush_controller_->beginStroke();
    brush_controller_->continueStroke(pos);
    session->setModified(true);
    spdlog::debug("[INPUT] Started brush drag stroke at ({}, {}, {})", pos.x,
                  pos.y, pos.z);
    return;
  }

  is_dragging_ = true;
  drag_start_pos_ = pos;
  spdlog::debug("[INPUT] Drag Start at ({},{},{})", pos.x, pos.y, pos.z);
}

void MapInputController::onLeftDragEnd(const Domain::Position &pos,
                                       EditorSession *session) {
  // BRUSH MODE: End stroke
  if (is_brush_dragging_) {
    is_brush_dragging_ = false;
    if (brush_controller_) {
      brush_controller_->endStroke();
      spdlog::debug("[INPUT] Ended brush drag stroke");
    }
    return;
  }

  if (!session || !is_dragging_)
    return;

  is_dragging_ = false;

  auto &selection_service = session->getSelectionService();
  if (selection_service.isEmpty())
    return;

  auto *map = session->getMap();
  if (!map)
    return;

  // Position delta
  int32_t const dx = pos.x - drag_start_pos_.x;
  int32_t const dy = pos.y - drag_start_pos_.y;

  if (dx == 0 && dy == 0)
    return;

  // Delegate business logic to MapEditingService
  Services::MapEditingService editing_service;
  if (editing_service.moveItems(map, selection_service,
                                session->getHistoryManager(), dx, dy)) {
    session->setModified(true);
  }
}

void MapInputController::onRightClick(const Domain::Position &pos,
                                      EditorSession *session) {
  if (!session)
    return;

  // Clear brush on right-click
  if (brush_controller_ && brush_controller_->hasBrush()) {
    brush_controller_->clearBrush();
    spdlog::debug(
        "[INPUT] Right-click: cleared brush, switched to selection mode");
    return;
  }

  context_menu_pos_ = pos;
  show_context_menu_ = true;
}

void MapInputController::onDoubleClick(const Domain::Position &pos,
                                       const glm::vec2 &pixel_offset,
                                       EditorSession *session) {
  if (!session)
    return;

  auto *map = session->getMap();
  if (!map)
    return;

  auto *tile = map->getTile(pos);
  if (!tile)
    return;

  // Priority 1: Spawn
  if (tile->hasSpawn()) {
    auto *spawn = tile->getSpawn();
    if (spawn && open_spawn_properties_callback_) {
      open_spawn_properties_callback_(spawn, pos);
      return;
    }
  }

  // Priority 2: Creature
  if (tile->hasCreature()) {
    Domain::Creature *creature = tile->getCreature();
    if (creature && open_creature_properties_callback_) {
      open_creature_properties_callback_(creature, creature->name, pos);
      return;
    }
  }

  // Priority 3: Item
  ensureCorrectStrategy();
  auto entry = current_strategy_->selectAt(map, pos, pixel_offset);

  if (entry.getType() == EntityType::Item ||
      entry.getType() == EntityType::Ground) {
    const Domain::Item *item_ptr =
        static_cast<const Domain::Item *>(entry.entity_ptr);
    if (item_ptr && open_item_properties_callback_) {
      Domain::Tile  const*mutable_tile = map->getTile(entry.getPosition());
      if (mutable_tile) {
        auto &items = mutable_tile->getItems();
        for (auto &item : items) {
          if (item.get() == item_ptr) {
            open_item_properties_callback_(item.get());
            break;
          }
        }
      }
    }
  }

  // Select it
  auto &selection_service = session->getSelectionService();
  selection_service.clear();
  selection_service.addEntity(entry);
}

void MapInputController::onMouseMove(const Domain::Position &pos,
                                     EditorSession *session) {
  if (!is_brush_dragging_ || !brush_controller_ || !session) {
    return;
  }

  if (pos == last_brush_pos_) {
    return;
  }

  last_brush_pos_ = pos;
  brush_controller_->continueStroke(pos);
  session->setModified(true);
}

bool MapInputController::hasBrush() const {
  return brush_controller_ && brush_controller_->hasBrush();
}

} // namespace MapEditor::AppLogic
