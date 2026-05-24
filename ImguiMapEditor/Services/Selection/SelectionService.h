#pragma once
#include "Domain/Selection/SelectionBucket.h"
#include "Domain/Selection/SelectionFilter.h"
#include "Domain/Selection/SelectionSnapshot.h"
#include "ISelectionObserver.h"
#include <memory>
#include <vector>

// Forward declarations
namespace MapEditor::Domain {
class Tile;
class ChunkedMap;
class Item;
struct Creature;
struct Spawn;
} // namespace MapEditor::Domain

namespace MapEditor::Services::Selection {

/**
 * Central service for all selection operations.
 * Follows Single Responsibility: selection logic only.
 *
 * Features:
 * - Unified API for all selection methods (click, drag, lasso)
 * - Granular entity-level selection/deselection
 * - Snapshot support for undo/redo
 * - Observer pattern for reactive UI
 *
 * Design principles:
 * - No rendering logic
 * - No input handling logic
 * - Just pure selection state management
 *
 * Thread safety: NOT thread-safe. Caller must synchronize.
 */
class SelectionService {
public:
  SelectionService() = default;
  ~SelectionService() = default;

  // Non-copyable (owns observers list)
  SelectionService(const SelectionService &) = delete;
  SelectionService &operator=(const SelectionService &) = delete;

  // Movable
  SelectionService(SelectionService &&) noexcept = default;
  SelectionService &operator=(SelectionService &&) noexcept = default;

  // === Selection Operations ===

  /**
   * Select entities at a position based on filter.
   *
   * @param map Map to read tile data from
   * @param pos Target position
   * @param filter Which entities to select
   * @param clear_first If true, clears existing selection first
   */
  void selectAt(Domain::ChunkedMap *map, const Domain::Position &pos,
                const Domain::Selection::SelectionFilter &filter,
                bool clear_first = false);

  /**
   * Select entities in a rectangular region.
   * Iterates through all tiles in the region and selects based on filter.
   */
  void selectRegion(Domain::ChunkedMap *map, int32_t min_x, int32_t min_y,
                    int32_t max_x, int32_t max_y, int16_t z,
                    const Domain::Selection::SelectionFilter &filter);

  /**
   * Select all entities on a single tile.
   * Convenience method that creates filter for all entity types.
   */
  void selectTile(Domain::ChunkedMap *map, const Domain::Position &pos);

  /**
   * Deselect entities at a position based on filter.
   */
  void deselectAt(const Domain::Position &pos,
                  const Domain::Selection::SelectionFilter &filter);

  /**
   * Toggle selection state of entities.
   * If selected → deselect. If not selected → select.
   */
  void toggleAt(Domain::ChunkedMap *map, const Domain::Position &pos,
                const Domain::Selection::SelectionFilter &filter);

  /**
   * Clear entire selection.
   */
  void clear();

  // === Direct Entity Operations (for fine-grained control) ===

  /**
   * Add a single entity to selection.
   */
  void addEntity(const Domain::Selection::SelectionEntry &entry);

  /**
   * Remove a single entity from selection.
   */
  void removeEntity(const Domain::Selection::EntityId &id);

  /**
   * Toggle a single entity.
   */
  void toggleEntity(Domain::ChunkedMap *map,
                    const Domain::Selection::SelectionEntry &entry);

  // === Query ===

  bool isEmpty() const { return bucket_.empty(); }
  size_t size() const { return bucket_.size(); }

  bool isSelected(const Domain::Selection::EntityId &id) const;
  bool hasSelectionAt(const Domain::Position &pos) const;

  std::vector<Domain::Selection::SelectionEntry>
  getEntriesAt(const Domain::Position &pos) const;
  std::vector<Domain::Selection::SelectionEntry> getAllEntries() const;
  std::vector<Domain::Position> getPositions() const;

  Domain::Position getMinBound() const { return bucket_.getMinBound(); }
  Domain::Position getMaxBound() const { return bucket_.getMaxBound(); }

  // Floor-filtered queries
  std::vector<Domain::Selection::SelectionEntry>
  getEntriesOnFloor(int16_t floor) const;

  // === Snapshot (for Undo/Redo) ===

  /**
   * Create a snapshot of current selection state.
   */
  Domain::Selection::SelectionSnapshot createSnapshot() const;

  /**
   * Restore selection from a snapshot.
   * Replaces current selection entirely.
   */
  void restoreSnapshot(const Domain::Selection::SelectionSnapshot &snapshot);

  // === Observer Pattern ===

  /**
   * Add an observer to be notified of selection changes.
   * Observer is not owned - caller must manage lifetime.
   */
  void addObserver(ISelectionObserver *observer);

  /**
   * Remove an observer.
   */
  void removeObserver(ISelectionObserver *observer);

  // === Bulk Operations (for efficiency) ===

  /**
   * Add multiple entities from a tile based on filter.
   * More efficient than multiple individual adds.
   */
  void addTileEntities(const Domain::Tile *tile,
                       const Domain::Selection::SelectionFilter &filter);

  /**
   * Remove all entities at a position.
   */
  void removeAllAt(const Domain::Position &pos);

private:
  Domain::Selection::SelectionBucket bucket_;
  std::vector<ISelectionObserver *> observers_;

  // Helper: create SelectionEntry for ground
  static Domain::Selection::SelectionEntry
  createGroundEntry(const Domain::Position &pos, const Domain::Item *ground);

  // Helper: create SelectionEntry for item
  static Domain::Selection::SelectionEntry createItemEntry(const Domain::Position &pos,
                                                    const Domain::Item *item);

  // Helper: create SelectionEntry for creature
  static Domain::Selection::SelectionEntry
  createCreatureEntry(const Domain::Position &pos,
                      const Domain::Creature *creature);

  // Helper: create SelectionEntry for spawn
  static Domain::Selection::SelectionEntry
  createSpawnEntry(const Domain::Position &pos, const Domain::Spawn *spawn);

  // Helper: notify observers of changes
  void
  notifyChanged(const std::vector<Domain::Selection::SelectionEntry> &added,
                const std::vector<Domain::Selection::SelectionEntry> &removed);

  // Helper: notify observers of clear
  void notifyCleared();
};

} // namespace MapEditor::Services::Selection
