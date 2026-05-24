#include "HistoryManager.h"
#include "Services/Selection/SelectionService.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Domain::History {

HistoryManager::HistoryManager(const HistoryConfig& config)
    : buffer_(config) {
}

void HistoryManager::beginOperation(const std::string& description, ActionType type,
                                     Services::Selection::SelectionService* selection) {
    if (operation_active_) {
        spdlog::warn("[History] beginOperation called while operation already active, canceling previous");
        cancelOperation();
    }
    
    operation_active_ = true;
    current_description_ = description;
    current_type_ = type;
    before_states_.clear();
    
    // Capture selection state before operation (if provided)
    if (selection) {
        selection_before_ = selection->createSnapshot();
    } else {
        selection_before_.reset();
    }
    
    spdlog::debug("[History] Begin operation: {}", description);
}

void HistoryManager::recordTileBefore(const Position& pos, const Tile* tile) {
    if (!operation_active_) {
        spdlog::warn("[History] recordTileBefore called without active operation");
        return;
    }
    
    // Only capture first time for this position (BEFORE state)
    if (before_states_.find(pos) != before_states_.end()) {
        return;  // Already captured
    }
    
    before_states_[pos] = TileSnapshot::capture(tile, pos);
}

void HistoryManager::endOperation(ChunkedMap* map, Services::Selection::SelectionService* selection) {
    if (!operation_active_) {
        spdlog::warn("[History] endOperation called without active operation");
        return;
    }
    
    // Check if we have any changes (tiles or selection)
    bool const has_tile_changes = !before_states_.empty();
    bool const has_selection_changes = selection_before_.has_value();
    
    if (!has_tile_changes && !has_selection_changes) {
        // No changes recorded
        operation_active_ = false;
        before_states_.clear();
        selection_before_.reset();
        return;
    }
    
    // Create history entry
    auto entry = std::make_unique<HistoryEntry>(current_description_, current_type_);
    
    // Add BEFORE and AFTER tile snapshots
    for (auto& [pos, before_snapshot] : before_states_) {
        entry->addBeforeSnapshot(std::move(before_snapshot));
        
        // Capture AFTER state from current map
        const Tile* current_tile = map->getTile(pos);
        entry->addAfterSnapshot(TileSnapshot::capture(current_tile, pos));
    }
    
    // Add selection snapshots if captured
    if (selection_before_.has_value()) {
        entry->setSelectionBefore(*selection_before_);
        if (selection) {
            entry->setSelectionAfter(selection->createSnapshot());
        }
    }
    
    // Push to buffer
    buffer_.push(std::move(entry));
    
    // Reset state
    operation_active_ = false;
    before_states_.clear();
    selection_before_.reset();
    
    spdlog::debug("[History] End operation: {}", current_description_);
}

void HistoryManager::cancelOperation() {
    operation_active_ = false;
    before_states_.clear();
    selection_before_.reset();
    spdlog::debug("[History] Operation canceled");
}

void HistoryManager::recordSingleTileChange(
    ChunkedMap* map,
    const Position& pos,
    const std::string& description,
    ActionType type
) {
    // Capture BEFORE
    const Tile* before_tile = map->getTile(pos);
    TileSnapshot before_snapshot = TileSnapshot::capture(before_tile, pos);
    
    // Note: Caller modifies tile between this call and next
    // This is a convenience for single-tile atomic changes
    // For proper use, caller should:
    // 1. Call beginOperation
    // 2. Call recordTileBefore
    // 3. Make changes
    // 4. Call endOperation
    
    // For now, this just sets up for a follow-up call
    beginOperation(description, type);
    before_states_[pos] = std::move(before_snapshot);
}

std::string HistoryManager::undo(ChunkedMap* map, Services::ClientDataService* clientData,
                                  Services::Selection::SelectionService* selection) {
    HistoryEntry* entry = buffer_.moveBack();
    if (!entry) {
        return "";
    }
    
    std::string desc = entry->getDescription();
    entry->undo(map, clientData, selection);
    
    spdlog::debug("[History] Undo: {}", desc);
    return desc;
}

std::string HistoryManager::redo(ChunkedMap* map, Services::ClientDataService* clientData,
                                  Services::Selection::SelectionService* selection) {
    HistoryEntry* entry = buffer_.moveForward();
    if (!entry) {
        return "";
    }
    
    std::string desc = entry->getDescription();
    entry->redo(map, clientData, selection);
    
    spdlog::debug("[History] Redo: {}", desc);
    return desc;
}

} // namespace MapEditor::Domain::History
