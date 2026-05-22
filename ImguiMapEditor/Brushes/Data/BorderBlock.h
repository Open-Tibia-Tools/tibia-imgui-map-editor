#pragma once
/**
 * @file BorderBlock.h
 * @brief Data structures for border item storage and specific case handling.
 */

#include "../Enums/BrushEnums.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>


namespace MapEditor::Brushes {

/**
 * Specific case block for complex border conditions.
 * Used by GroundBrush to handle special border situations.
 */
class SpecificCaseBlock {
public:
  void addMatchItem(uint32_t itemId);
  void setMatchGroup(uint32_t matchGroup, EdgeType alignment) {
    matchGroup_ = matchGroup;
    groupMatchAlignment_ = alignment;
  }
  void setReplaceAction(uint32_t toReplaceId, uint32_t withId);
  void setDeleteAll(bool value) { deleteAll_ = value; }
  void setKeepBorder(bool value) { keepBorder_ = value; }

  const std::vector<uint32_t> &getItemsToMatch() const { return itemsToMatch_; }
  uint32_t getMatchGroup() const { return matchGroup_; }
  EdgeType getGroupMatchAlignment() const { return groupMatchAlignment_; }
  size_t getRequiredMatchCount() const {
    return itemsToMatch_.size() + (matchGroup_ != 0 ? 1u : 0u);
  }
  uint32_t getToReplaceId() const { return toReplaceId_; }
  uint32_t getWithId() const { return withId_; }
  bool isDeleteAll() const { return deleteAll_; }
  bool keepBorder() const { return keepBorder_; }

private:
  std::vector<uint32_t> itemsToMatch_;
  uint32_t matchGroup_ = 0;
  EdgeType groupMatchAlignment_ = EdgeType::None;
  uint32_t toReplaceId_ = 0;
  uint32_t withId_ = 0;
  bool deleteAll_ = false;
  bool keepBorder_ = false;
};

/**
 * Stores border items for each edge type.
 * Used by GroundBrush for border generation.
 */
class BorderBlock {
public:
  static constexpr size_t kEdgeTypeCount =
      14; // EdgeType::None through EdgeType::Center

  /**
   * Add an item for a specific edge type.
   * @param edge The edge type
   * @param itemId The item ID to place
   * @param chance Weight for random selection (higher = more likely)
   */
  void addItem(EdgeType edge, uint32_t itemId, uint32_t chance);

  /**
   * Check if this block has items for a specific edge type.
   */
  bool hasItemsFor(EdgeType edge) const;

  /**
   * Get a random item for a specific edge type using weighted selection.
   * @return Item ID, or 0 if no items available
   */
  uint32_t getRandomItem(EdgeType edge) const;

  /**
   * Get the canonical item for a specific edge type.
   * Used by reference-style specific-case matching where edge identity must be
   * stable and not depend on weighted placement selection.
   */
  uint32_t getPrimaryItem(EdgeType edge) const;

  /**
   * Get all items for a specific edge type.
   */
  const std::vector<std::pair<uint32_t, uint32_t>> &
  getItems(EdgeType edge) const;

  /**
   * Set the owner brush name (for z-order comparison).
   */
  void setOwnerBrush(const std::string &name) { ownerBrush_ = name; }
  const std::string &getOwnerBrush() const { return ownerBrush_; }

  /**
   * Set ground equivalent ID for optional borders.
   */
  void setGroundEquivalent(uint32_t id) { groundEquivalent_ = id; }
  uint32_t getGroundEquivalent() const { return groundEquivalent_; }
  void setGroup(uint16_t group) { group_ = group; }
  uint16_t getGroup() const { return group_; }

  void addSpecificCase(SpecificCaseBlock specificCase);
  const std::vector<SpecificCaseBlock> &getSpecificCases() const {
    return specificCases_;
  }

private:
  // Items for each of 14 edge types: vector of (itemId, chance)
  std::array<std::vector<std::pair<uint32_t, uint32_t>>, kEdgeTypeCount> items_;

  // Reference to owner brush (for z-order comparison)
  std::string ownerBrush_;

  // Ground equivalent for optional borders
  uint32_t groundEquivalent_ = 0;
  uint16_t group_ = 0;

  std::vector<SpecificCaseBlock> specificCases_;

  // Random number generator for weighted selection
  mutable std::mt19937 rng_{std::random_device{}()};
};
} // namespace MapEditor::Brushes
