#include "GroundBrush.h"

#include "Brushes/BrushRegistry.h"
#include "BrushUtils.h"
#include "Brushes/Behaviors/WeightedSelection.h"
#include "Brushes/Enums/BrushEnums.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Tile.h"
#include "Services/Brushes/BorderLookupService.h"
#include "Services/ClientDataService.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <ranges>

namespace MapEditor::Brushes {

namespace {

struct AltGroundReplaceState {
  bool active = false;
  bool emptyOnly = false;
  const GroundBrush *replaceBrush = nullptr;
};

thread_local AltGroundReplaceState g_altGroundReplaceState {};

constexpr std::array<std::tuple<int, int, TileNeighbor>, 8> kNeighborOffsets{{
    {-1, -1, TileNeighbor::Northwest},
    {0, -1, TileNeighbor::North},
    {1, -1, TileNeighbor::Northeast},
    {-1, 0, TileNeighbor::West},
    {1, 0, TileNeighbor::East},
    {-1, 1, TileNeighbor::Southwest},
    {0, 1, TileNeighbor::South},
    {1, 1, TileNeighbor::Southeast},
}};

std::string normalizeName(const std::string &value) {
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return normalized;
}

[[nodiscard]] const GroundBrush *resolveGroundBrush(const BrushRegistry &registry,
                                                    const Domain::Tile &tile) {
  if (tile.getGroundBrushId() != InvalidBrushId) {
    if (const auto *brush =
            dynamic_cast<const GroundBrush *>(registry.getBrushById(tile.getGroundBrushId()))) {
      return brush;
    }
  }

  const auto *ground = tile.getGround();
  if (!ground) {
    return nullptr;
  }

  if (ground->getOwnerBrushId() != InvalidBrushId) {
    if (const auto *brush = dynamic_cast<const GroundBrush *>(
            registry.getBrushById(ground->getOwnerBrushId()))) {
      return brush;
    }
  }

  return dynamic_cast<const GroundBrush *>(
      registry.getBrushForItem(ground->getServerId()));
}

void resetAltGroundReplaceState() {
  g_altGroundReplaceState = {};
}

[[nodiscard]] bool shouldSkipAltGroundPlacement(const BrushRegistry &registry,
                                                const Domain::Tile &tile,
                                                const DrawContext &ctx) {
  const bool altPressed = (ctx.modifiers & GLFW_MOD_ALT) != 0;
  if (!altPressed) {
    resetAltGroundReplaceState();
    return false;
  }

  if (!g_altGroundReplaceState.active || !ctx.isDragging) {
    g_altGroundReplaceState.active = true;
    g_altGroundReplaceState.emptyOnly = !tile.hasGround();
    g_altGroundReplaceState.replaceBrush = nullptr;

    if (!g_altGroundReplaceState.emptyOnly) {
      g_altGroundReplaceState.replaceBrush = resolveGroundBrush(registry, tile);
      if (!g_altGroundReplaceState.replaceBrush) {
        resetAltGroundReplaceState();
        return true;
      }
    }
  }

  if (g_altGroundReplaceState.emptyOnly) {
    return tile.hasGround();
  }

  const auto *currentBrush = resolveGroundBrush(registry, tile);
  if (!currentBrush) {
    return true;
  }

  return currentBrush != g_altGroundReplaceState.replaceBrush;
}

} // namespace

GroundBrush::GroundBrush(std::string name, uint32_t lookId, BrushRegistry &registry)
    : BrushBase(std::move(name), lookId, true), registry_(registry) {}

void GroundBrush::resetAltReplaceState() { resetAltGroundReplaceState(); }

void GroundBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                       const DrawContext &ctx) {
  if (!tile || groundItems_.empty()) {
    return;
  }

  if (shouldSkipAltGroundPlacement(registry_, *tile, ctx)) {
    return;
  }

  const uint16_t itemId = selectWeightedItem(groundItems_);
  if (itemId == 0) {
    return;
  }

  auto groundItem = Types::createTypedItem(ctx, itemId);
  tile->setGround(std::move(groundItem));
  tile->setGroundBrushId(ctx.ownerBrushId);

  rebuildAround(map, tile->getPosition());

  if (!ctx.isDragging) {
    resetAltGroundReplaceState();
  }
}

void GroundBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile) {
    return;
  }

  if (const auto *ground = tile->getGround(); ground && ownsItem(ground)) {
    tile->removeGround();
  }
  tile->setGroundBrushId(InvalidBrushId);

  tile->removeItemsIf([this](const Domain::Item *item) {
    return item && isBorderItem(item->getServerId());
  });
  tile->setOptionalBorder(false);

  rebuildAround(map, tile->getPosition());
}

bool GroundBrush::ownsItem(const Domain::Item *item) const {
  return item && ownedItemIds_.contains(item->getServerId());
}

void GroundBrush::addGroundItem(uint16_t itemId, uint32_t chance) {
  groundItems_.emplace_back(itemId, chance == 0 ? 1u : chance);
  ownedItemIds_.insert(itemId);
  registry_.registerItemBinding(itemId, this);
  if (lookId_ == 0) {
    lookId_ = itemId;
  }
}

void GroundBrush::addFriend(const std::string &name) {
  friendNames_.insert(normalizeName(name));
}

void GroundBrush::addEnemy(const std::string &name) {
  enemyNames_.insert(normalizeName(name));
}

void GroundBrush::addBorderRule(BorderRule rule) {
  const auto registerOwnedItem = [this](uint32_t itemId) {
    if (itemId == 0 || itemId > std::numeric_limits<uint16_t>::max()) {
      return;
    }

    const auto ownedItemId = static_cast<uint16_t>(itemId);
    ownedItemIds_.insert(ownedItemId);
    registry_.registerItemBinding(ownedItemId, this);
  };

  for (size_t i = 0; i < BorderBlock::kEdgeTypeCount; ++i) {
    const auto edge = static_cast<EdgeType>(i);
    if (!rule.block.hasItemsFor(edge)) {
      continue;
    }
    for (const auto &[itemId, _] : rule.block.getItems(edge)) {
      registerOwnedItem(itemId);
    }
  }

  for (const auto &specificCase : rule.block.getSpecificCases()) {
    registerOwnedItem(specificCase.getToReplaceId());
    registerOwnedItem(specificCase.getWithId());
  }

  borderRules_.push_back(std::move(rule));
}

void GroundBrush::setOptionalBorder(BorderBlock border, bool soloOptional) {
  soloOptionalBorder_ = soloOptional;
  const auto registerOwnedItem = [this](uint32_t itemId) {
    if (itemId == 0 || itemId > std::numeric_limits<uint16_t>::max()) {
      return;
    }

    const auto ownedItemId = static_cast<uint16_t>(itemId);
    ownedItemIds_.insert(ownedItemId);
    registry_.registerItemBinding(ownedItemId, this);
  };

  for (size_t i = 0; i < BorderBlock::kEdgeTypeCount; ++i) {
    const auto edge = static_cast<EdgeType>(i);
    if (!border.hasItemsFor(edge)) {
      continue;
    }
    for (const auto &[itemId, _] : border.getItems(edge)) {
      registerOwnedItem(itemId);
    }
  }

  for (const auto &specificCase : border.getSpecificCases()) {
    registerOwnedItem(specificCase.getToReplaceId());
    registerOwnedItem(specificCase.getWithId());
  }

  optionalBorder_ = std::move(border);
}

uint16_t GroundBrush::getPreviewItemId() const {
  return groundItems_.empty() ? 0 : groundItems_.front().first;
}

void GroundBrush::rebuildAround(Domain::ChunkedMap &map,
                                const Domain::Position &center) const {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      rebuildTile(map, {center.x + dx, center.y + dy, center.z});
    }
  }
}

void GroundBrush::rebuildTile(Domain::ChunkedMap &map,
                              const Domain::Position &pos) const {
  auto *tile = map.getTile(pos);
  if (!tile || !tile->hasGround()) {
    return;
  }

  auto *brush = const_cast<GroundBrush *>(resolveGroundBrush(registry_, *tile));
  if (!brush) {
    return;
  }

  brush->updateBorderItems(map, *tile);
}

const GroundBrush::BorderRule *GroundBrush::findRuleFor(
    const GroundBrush *other, bool requireOuter) const {
  const std::string otherName =
      other ? normalizeName(other->getName()) : std::string{};

  for (const auto &rule : borderRules_) {
    if (rule.outer != requireOuter) {
      continue;
    }
    if (!other) {
      if (rule.targetNone) {
        return &rule;
      }
      continue;
    }
    if (rule.targetName.empty() || normalizeName(rule.targetName) == otherName ||
        normalizeName(rule.targetName) == "all") {
      return &rule;
    }
  }
  return nullptr;
}

bool GroundBrush::connectsTo(const GroundBrush *other) const {
  if (other == nullptr) {
    return false;
  }
  if (other == this) {
    return true;
  }

  const auto otherName = normalizeName(other->getName());
  if (enemyNames_.contains("all") || enemyNames_.contains(otherName)) {
    return false;
  }
  if (friendNames_.contains("all") || friendNames_.contains(otherName)) {
    return true;
  }
  return other->friendNames_.contains("all") || other->friendNames_.contains(normalizeName(getName()));
}

bool GroundBrush::isFriendName(const std::string &name) const {
  return friendNames_.contains(normalizeName(name));
}

uint16_t GroundBrush::selectWeightedItem(
    const std::vector<std::pair<uint16_t, uint32_t>> &items) const {
  if (items.empty()) {
    return 0;
  }

  std::vector<uint32_t> weights;
  weights.reserve(items.size());
  for (const auto &[_, chance] : items) {
    weights.push_back(chance == 0 ? 1u : chance);
  }

  const auto selected = WeightedSelection::select(weights);
  return selected ? items[*selected].first : items.front().first;
}

void GroundBrush::updateBorderItems(Domain::ChunkedMap &map,
                                    Domain::Tile &tile) const {
  struct NeighborState {
    bool visited = false;
    const GroundBrush *brush = nullptr;
  };

  struct BorderCluster {
    TileNeighbor alignment = TileNeighbor::None;
    int zOrder = 0;
    const BorderBlock *block = nullptr;
    const GroundBrush *ownerBrush = nullptr;
  };

  struct ResolvedBorderRule {
    const BorderBlock *block = nullptr;
    const GroundBrush *ownerBrush = nullptr;
    int zOrder = 0;
  };

  tile.removeItemsIf([this](const Domain::Item *item) {
    if (!item) {
      return false;
    }
    auto *boundBrush = registry_.getBrushForItem(item->getServerId());
    auto *groundBrush = dynamic_cast<const GroundBrush *>(boundBrush);
    return groundBrush && groundBrush->isBorderItem(item->getServerId());
  });

  const auto hasBorderRule = [](const GroundBrush *brush, bool outerRule) {
    return brush &&
           std::ranges::any_of(brush->borderRules_, [outerRule](const BorderRule &rule) {
             return rule.outer == outerRule;
           });
  };

  const auto hasZilchBorderRule = [](const GroundBrush *brush, bool outerRule) {
    return brush &&
           std::ranges::any_of(brush->borderRules_, [outerRule](const BorderRule &rule) {
             return rule.outer == outerRule && rule.targetNone;
           });
  };

  const auto findRule = [](const GroundBrush *brush, bool outerRule,
                           const GroundBrush *other) -> const BorderRule * {
    if (!brush) {
      return nullptr;
    }

    const auto otherName = other ? normalizeName(other->getName()) : std::string{};
    for (const auto &rule : brush->borderRules_) {
      if (rule.outer != outerRule) {
        continue;
      }

      if (!other) {
        if (rule.targetNone) {
          return &rule;
        }
        continue;
      }

      if (rule.targetNone) {
        continue;
      }

      const auto targetName = normalizeName(rule.targetName);
      if (targetName.empty() || targetName == otherName || targetName == "all") {
        return &rule;
      }
    }

    return nullptr;
  };

  const auto resolveRuleTo = [&](const GroundBrush *centerBrush,
                                 const GroundBrush *neighborBrush)
      -> std::optional<ResolvedBorderRule> {
    if (!centerBrush) {
      return std::nullopt;
    }

    if (!neighborBrush) {
      if (!hasZilchBorderRule(centerBrush, false)) {
        return std::nullopt;
      }

      if (const auto *rule = findRule(centerBrush, false, nullptr)) {
        return ResolvedBorderRule{
            .block = &rule->block,
            .ownerBrush = centerBrush,
            .zOrder = -1000,
        };
      }

      return std::nullopt;
    }

    const auto *lowerBrush = centerBrush;
    const auto *higherBrush = neighborBrush;
    if (centerBrush->getZOrder() > neighborBrush->getZOrder()) {
      lowerBrush = neighborBrush;
      higherBrush = centerBrush;
    }

    const auto *lowerRule = findRule(lowerBrush, true, higherBrush);
    const auto *higherRule = findRule(higherBrush, false, lowerBrush);

    if (lowerRule) {
      return ResolvedBorderRule{
          .block = &lowerRule->block,
          .ownerBrush = lowerBrush,
          .zOrder = lowerBrush->getZOrder(),
      };
    }

    if (higherRule) {
      return ResolvedBorderRule{
          .block = &higherRule->block,
          .ownerBrush = higherBrush,
          .zOrder = higherBrush->getZOrder(),
      };
    }

    return std::nullopt;
  };

  const auto appendCluster = [](std::vector<BorderCluster> &clusters,
                                TileNeighbor alignment, int zOrder,
                                const BorderBlock *block,
                                const GroundBrush *ownerBrush) {
    if (alignment == TileNeighbor::None || !block) {
      return;
    }

    if (auto it = std::ranges::find_if(
            clusters, [block, ownerBrush, zOrder](const BorderCluster &cluster) {
              return cluster.block == block && cluster.ownerBrush == ownerBrush &&
                     cluster.zOrder == zOrder;
            });
        it != clusters.end()) {
      it->alignment |= alignment;
      return;
    }

    clusters.push_back(BorderCluster{
        .alignment = alignment,
        .zOrder = zOrder,
        .block = block,
        .ownerBrush = ownerBrush,
    });
  };

  const auto addEdgeItems = [&](const BorderBlock &block, EdgeType edge,
                                const GroundBrush *ownerBrush) {
    if (!block.hasItemsFor(edge)) {
      return false;
    }

    const auto itemId = static_cast<uint16_t>(block.getRandomItem(edge));
    if (itemId == 0) {
      return false;
    }

    DrawContext borderCtx;
    borderCtx.clientData = registry_.getClientDataService();
    borderCtx.brushRegistry = &registry_;
    borderCtx.ownerBrushId = registry_.getBrushId(ownerBrush);
    tile.addItem(Types::createTypedItem(borderCtx, itemId));
    return true;
  };

  const auto addEdgeWithFallback = [&](const BorderBlock &block, EdgeType edge,
                                       const GroundBrush *ownerBrush) {
    if (addEdgeItems(block, edge, ownerBrush)) {
      return;
    }

    switch (edge) {
    case EdgeType::DNW:
      if (block.hasItemsFor(EdgeType::W) && block.hasItemsFor(EdgeType::N)) {
        addEdgeItems(block, EdgeType::W, ownerBrush);
        addEdgeItems(block, EdgeType::N, ownerBrush);
      }
      break;
    case EdgeType::DNE:
      if (block.hasItemsFor(EdgeType::E) && block.hasItemsFor(EdgeType::N)) {
        addEdgeItems(block, EdgeType::E, ownerBrush);
        addEdgeItems(block, EdgeType::N, ownerBrush);
      }
      break;
    case EdgeType::DSW:
      if (block.hasItemsFor(EdgeType::S) && block.hasItemsFor(EdgeType::W)) {
        addEdgeItems(block, EdgeType::S, ownerBrush);
        addEdgeItems(block, EdgeType::W, ownerBrush);
      }
      break;
    case EdgeType::DSE:
      if (block.hasItemsFor(EdgeType::S) && block.hasItemsFor(EdgeType::E)) {
        addEdgeItems(block, EdgeType::S, ownerBrush);
        addEdgeItems(block, EdgeType::E, ownerBrush);
      }
      break;
    default:
      break;
    }
  };

  const auto pos = tile.getPosition();
  const auto *borderBrush = this;
  std::array<NeighborState, kNeighborOffsets.size()> neighbors {};
  for (size_t index = 0; index < kNeighborOffsets.size(); ++index) {
    const auto &[dx, dy, _] = kNeighborOffsets[index];
    const auto *neighborTile = map.getTile(pos.x + dx, pos.y + dy, pos.z);
    neighbors[index].brush =
        neighborTile ? resolveGroundBrush(registry_, *neighborTile) : nullptr;
  }

  std::vector<BorderCluster> clusters;
  clusters.reserve(8);

  for (size_t index = 0; index < neighbors.size(); ++index) {
    if (neighbors[index].visited) {
      continue;
    }

    const auto *other = neighbors[index].brush;
    TileNeighbor alignment = TileNeighbor::None;
    for (size_t probe = index; probe < neighbors.size(); ++probe) {
      if (!neighbors[probe].visited && neighbors[probe].brush == other) {
        neighbors[probe].visited = true;
        alignment |= std::get<2>(kNeighborOffsets[probe]);
      }
    }

    if (alignment == TileNeighbor::None) {
      continue;
    }

    if (other == borderBrush) {
      continue;
    }

    if (other) {
      bool onlyOptionalBorder = false;
      if ((other->connectsTo(borderBrush) || borderBrush->connectsTo(other)) &&
          other->hasOptionalBorderRule()) {
        onlyOptionalBorder = true;
      }

      if (tile.hasOptionalBorder() && other->optionalBorder_) {
        appendCluster(clusters, alignment, std::numeric_limits<int>::max(),
                      &*other->optionalBorder_, other);
        if (other->usesSoloOptionalBorder()) {
          onlyOptionalBorder = true;
        }
      }

      if (onlyOptionalBorder) {
        continue;
      }
    }

    if (const auto resolved = resolveRuleTo(borderBrush, other)) {
      appendCluster(clusters, alignment, resolved->zOrder, resolved->block,
                    resolved->ownerBrush);
    }
  }

  std::ranges::sort(clusters, [](const BorderCluster &lhs,
                                 const BorderCluster &rhs) {
    return lhs.zOrder < rhs.zOrder;
  });

  struct SpecificCaseWorkItem {
    const SpecificCaseBlock *specificCase = nullptr;
    const GroundBrush *ownerBrush = nullptr;
  };

  std::vector<SpecificCaseWorkItem> specificCases;
  for (const auto &cluster : clusters) {
    if (!cluster.block) {
      continue;
    }
    for (const auto &specificCase : cluster.block->getSpecificCases()) {
      specificCases.push_back(
          {.specificCase = &specificCase, .ownerBrush = cluster.ownerBrush});
    }
  }

  const Services::Brushes::BorderLookupService borderLookupService;
  for (auto it = clusters.rbegin(); it != clusters.rend(); ++it) {
    const auto packed = borderLookupService.getBorderTypes(it->alignment);
    for (const auto edge : Services::Brushes::BorderLookupService::unpack(packed)) {
      addEdgeWithFallback(*it->block, edge, it->ownerBrush);
    }
  }

  auto updateItemType = [this](Domain::Item *item, uint16_t itemId,
                               const GroundBrush *ownerBrush) {
    if (!item) {
      return;
    }

    item->setServerId(itemId);
    item->setOwnerBrushId(registry_.getBrushId(ownerBrush));
    if (const auto *clientData = registry_.getClientDataService()) {
      if (const auto *itemType = clientData->getItemTypeByServerId(itemId)) {
        item->setType(itemType);
        item->setClientId(itemType->client_id);
      }
    }
  };

  const auto isBorderLikeItem = [this](const Domain::Item *item) {
    if (!item) {
      return false;
    }

    if (registry_.getBorderItemMetadata(item->getServerId()) != nullptr) {
      return true;
    }

    const auto *boundBrush = registry_.getBrushForItem(item->getServerId());
    const auto *groundBrush = dynamic_cast<const GroundBrush *>(boundBrush);
    return groundBrush && groundBrush->isBorderItem(item->getServerId());
  };

  for (const auto &specificCaseWorkItem : specificCases) {
    const auto *specificCase = specificCaseWorkItem.specificCase;
    if (!specificCase) {
      continue;
    }

    const auto &itemsToMatch = specificCase->getItemsToMatch();
    if (itemsToMatch.empty()) {
      continue;
    }

    size_t matches = 0;
    for (size_t index = 0; index < tile.getItemCount(); ++index) {
      const auto *item = tile.getItem(index);
      if (!isBorderLikeItem(item)) {
        continue;
      }

      if (specificCase->getMatchGroup() != 0) {
        if (const auto *metadata =
                registry_.getBorderItemMetadata(item->getServerId());
            metadata && metadata->group == specificCase->getMatchGroup() &&
            metadata->alignment == specificCase->getGroupMatchAlignment()) {
          ++matches;
          continue;
        }
      }

      if (std::ranges::find(itemsToMatch, item->getServerId()) !=
          itemsToMatch.end()) {
        ++matches;
      }
    }

    if (matches < specificCase->getRequiredMatchCount()) {
      continue;
    }

    bool replaced = specificCase->isDeleteAll();
    size_t index = 0;
    while (index < tile.getItemCount()) {
      auto *item = tile.getItem(index);
      if (!isBorderLikeItem(item)) {
        ++index;
        continue;
      }

      if (std::ranges::find(itemsToMatch, item->getServerId()) ==
          itemsToMatch.end()) {
        ++index;
        continue;
      }

      if (!replaced && specificCase->getToReplaceId() != 0 &&
          item->getServerId() == specificCase->getToReplaceId() &&
          specificCase->getWithId() != 0) {
        updateItemType(item, static_cast<uint16_t>(specificCase->getWithId()),
                       specificCaseWorkItem.ownerBrush);
        replaced = true;
        ++index;
        continue;
      }

      if (specificCase->isDeleteAll() || !specificCase->keepBorder()) {
        tile.removeItem(index);
        continue;
      }

      ++index;
    }
  }
}

bool GroundBrush::isBorderItem(uint16_t itemId) const {
  return ownedItemIds_.contains(itemId) &&
         std::ranges::none_of(groundItems_, [itemId](const auto &entry) {
           return entry.first == itemId;
         });
}

} // namespace MapEditor::Brushes
