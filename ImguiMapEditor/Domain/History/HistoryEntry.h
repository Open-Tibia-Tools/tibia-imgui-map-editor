#pragma once
#include "TileSnapshot.h"
#include "TileSnapshotCodec.h"
#include "HistoryConfig.h"
#include "../ChunkedMap.h"
#include "../Position.h"
#include "../Selection/SelectionSnapshot.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace MapEditor::Services {
    class ClientDataService;
}

namespace MapEditor::Services::Selection {
    class SelectionService;
}

namespace MapEditor::Domain::History {

/**
 * Type of action for categorization and potential merging.
 */
enum class ActionType {
    Draw,           // Brush painting
    Move,           // Moving selection
    Paste,          // Pasting clipboard
    Delete,         // Deleting items
    Reorder,        // Reordering items
    Properties,     // Changing tile properties
    Spawn,          // Spawn/creature changes
    Other
};

/**
 * One undoable operation containing tile snapshots.
 * Stores BEFORE and AFTER states for all affected tiles.
 */
class HistoryEntry {
public:
    explicit HistoryEntry(const std::string& description, ActionType type = ActionType::Other);
    
    /**
     * Add a tile snapshot (stores BEFORE state).
     */
    void addBeforeSnapshot(TileSnapshot snapshot);
    
    /**
     * Add AFTER snapshot for a position.
     */
    void addAfterSnapshot(TileSnapshot snapshot);
    
    /**
     * Compress all snapshots (call after all snapshots added).
     */
    void compress(bool enable = true);
    
    /**
     * Apply undo - restore BEFORE states.
     * @param map The map to apply changes to
     * @param clientData Optional ClientDataService for resolving ItemTypes
     * @param selection Optional SelectionService to restore selection state
     */
    void undo(ChunkedMap* map, Services::ClientDataService* clientData = nullptr,
              Services::Selection::SelectionService* selection = nullptr);
    
    /**
     * Apply redo - restore AFTER states.
     * @param map The map to apply changes to
     * @param clientData Optional ClientDataService for resolving ItemTypes
     * @param selection Optional SelectionService to restore selection state
     */
    void redo(ChunkedMap* map, Services::ClientDataService* clientData = nullptr,
              Services::Selection::SelectionService* selection = nullptr);
    
    /**
     * Get description.
     */
    const std::string& getDescription() const { return description_; }
    
    /**
     * Get action type.
     */
    ActionType getType() const { return type_; }
    
    /**
     * Get total memory footprint.
     */
    size_t memsize() const;
    
    /**
     * Check if entry has any changes.
     */
    bool hasChanges() const { return !beforeSnapshots_.empty(); }
    
    /**
     * Get number of affected tiles.
     */
    size_t tileCount() const { return beforeSnapshots_.size(); }
    
    // === Selection State ===
    
    /**
     * Set selection state before operation.
     */
    void setSelectionBefore(const Domain::Selection::SelectionSnapshot& snapshot) {
        selection_before_ = snapshot;
    }
    
    /**
     * Set selection state after operation.
     */
    void setSelectionAfter(const Domain::Selection::SelectionSnapshot& snapshot) {
        selection_after_ = snapshot;
    }
    
    /**
     * Check if this entry has selection state changes.
     */
    bool hasSelectionChange() const { return selection_before_.has_value(); }
    
private:
    std::string description_;
    ActionType type_;
    
    // Snapshots stored as pairs: before[i] and after[i] correspond to same position
    std::vector<TileSnapshot> beforeSnapshots_;
    std::vector<TileSnapshot> afterSnapshots_;
    
    // Compression metadata
    std::vector<size_t> beforeOriginalSizes_;
    std::vector<size_t> afterOriginalSizes_;
    bool compressed_ = false;
    
    // Helper to restore snapshots to map and resolve ItemTypes
    void applySnapshots(ChunkedMap* map, const std::vector<TileSnapshot>& snapshots,
                        const std::vector<size_t>& originalSizes,
                        Services::ClientDataService* clientData) const;
    
    // Selection state (optional - only set if selection changed during operation)
    std::optional<Domain::Selection::SelectionSnapshot> selection_before_;
    std::optional<Domain::Selection::SelectionSnapshot> selection_after_;
};

} // namespace MapEditor::Domain::History
