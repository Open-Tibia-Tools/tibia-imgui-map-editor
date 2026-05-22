/**
 * @file BorderBlock.cpp
 * @brief Implementation of BorderBlock weighted selection.
 */

#include "BorderBlock.h"
#include <numeric>

namespace MapEditor::Brushes {

void BorderBlock::addItem(EdgeType edge, uint32_t itemId, uint32_t chance) {
  auto idx = static_cast<size_t>(edge);
  if (idx < kEdgeTypeCount) {
    items_[idx].emplace_back(itemId, chance);
  }
}

bool BorderBlock::hasItemsFor(EdgeType edge) const {
  auto idx = static_cast<size_t>(edge);
  return idx < kEdgeTypeCount && !items_[idx].empty();
}

uint32_t BorderBlock::getRandomItem(EdgeType edge) const {
  auto idx = static_cast<size_t>(edge);
  if (idx >= kEdgeTypeCount || items_[idx].empty()) {
    return 0;
  }

  const auto &edgeItems = items_[idx];

  // Calculate total weight
  uint32_t totalWeight = 0;
  for (const auto &[itemId, chance] : edgeItems) {
    totalWeight += chance;
  }

  if (totalWeight == 0) {
    return edgeItems.front().first;
  }

  // Weighted random selection
  std::uniform_int_distribution<uint32_t> dist(1, totalWeight);
  uint32_t roll = dist(rng_);

  uint32_t cumulative = 0;
  for (const auto &[itemId, chance] : edgeItems) {
    cumulative += chance;
    if (roll <= cumulative) {
      return itemId;
    }
  }

  return edgeItems.back().first;
}

uint32_t BorderBlock::getPrimaryItem(EdgeType edge) const {
  auto idx = static_cast<size_t>(edge);
  if (idx >= kEdgeTypeCount || items_[idx].empty()) {
    return 0;
  }

  return items_[idx].front().first;
}

const std::vector<std::pair<uint32_t, uint32_t>> &
BorderBlock::getItems(EdgeType edge) const {
  static const std::vector<std::pair<uint32_t, uint32_t>> empty;
  auto idx = static_cast<size_t>(edge);
  return idx < kEdgeTypeCount ? items_[idx] : empty;
}

void BorderBlock::addSpecificCase(SpecificCaseBlock specificCase) {
  specificCases_.push_back(std::move(specificCase));
}

void SpecificCaseBlock::addMatchItem(uint32_t itemId) {
  if (itemId != 0) {
    itemsToMatch_.push_back(itemId);
  }
}

void SpecificCaseBlock::setReplaceAction(uint32_t toReplaceId, uint32_t withId) {
  toReplaceId_ = toReplaceId;
  withId_ = withId;
}

} // namespace MapEditor::Brushes
