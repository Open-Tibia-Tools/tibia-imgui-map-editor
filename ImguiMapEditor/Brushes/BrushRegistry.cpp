#include "BrushRegistry.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Tile.h"
#include "Services/ClientDataService.h"
#include "Types/PlaceholderBrush.h"
#include "Types/RawBrush.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

std::string BrushRegistry::normalizeKey(const std::string &value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized;
}

// ========== Brush Management ==========

void BrushRegistry::addBrush(std::unique_ptr<IBrush> brush) {
    if (!brush) return;
    
    std::string name = brush->getName();
    const auto normalizedName = normalizeKey(name);
    BrushId brushId = nextBrushId_++;

    if (const auto idIt = brush_ids_by_name_.find(normalizedName); idIt != brush_ids_by_name_.end()) {
        brushId = idIt->second;
    }

    if (auto existing = named_brushes_.find(name); existing != named_brushes_.end()) {
        spdlog::warn("[BrushRegistry] Overwriting existing brush with name: {}", name);
        if (const auto ptrIt = brush_ids_.find(existing->second.get()); ptrIt != brush_ids_.end()) {
            brushId = ptrIt->second;
            brushes_by_id_[brushId] = nullptr;
            brush_ids_.erase(ptrIt);
        }
    }

    auto *brushPtr = brush.get();
    named_brushes_[name] = std::move(brush);
    brush_ids_[brushPtr] = brushId;
    brushes_by_id_[brushId] = brushPtr;
    brush_ids_by_name_[normalizedName] = brushId;
}

void BrushRegistry::registerExternalBrush(IBrush *brush) {
    if (!brush) {
        return;
    }

    const auto normalizedName = normalizeKey(brush->getName());
    BrushId brushId = nextBrushId_++;
    if (const auto idIt = brush_ids_by_name_.find(normalizedName); idIt != brush_ids_by_name_.end()) {
        brushId = idIt->second;
    }

    if (const auto ptrIt = brush_ids_.find(brush); ptrIt != brush_ids_.end()) {
        brushes_by_id_[ptrIt->second] = brush;
        brush_ids_by_name_[normalizedName] = ptrIt->second;
        return;
    }

    brush_ids_[brush] = brushId;
    brushes_by_id_[brushId] = brush;
    brush_ids_by_name_[normalizedName] = brushId;

    if (std::find(external_brushes_.begin(), external_brushes_.end(), brush) == external_brushes_.end()) {
        external_brushes_.push_back(brush);
    }
}

IBrush* BrushRegistry::getBrush(const std::string& name) const {
    auto it = named_brushes_.find(name);
    if (it != named_brushes_.end()) {
        return it->second.get();
    }

    const auto normalizedName = normalizeKey(name);
    if (const auto placeholderIt = placeholder_brushes_.find(normalizedName);
        placeholderIt != placeholder_brushes_.end()) {
        return placeholderIt->second.get();
    }

    if (const auto idIt = brush_ids_by_name_.find(normalizedName); idIt != brush_ids_by_name_.end()) {
        return getBrushById(idIt->second);
    }
    return nullptr;
}

IBrush* BrushRegistry::getOrCreateRAWBrush(uint16_t itemId) {
    auto it = raw_brushes_.find(itemId);
    if (it != raw_brushes_.end()) {
        return it->second.get();
    }

    const Domain::ItemType* itemType = nullptr;
    if (clientData_) {
        itemType = clientData_->getItemTypeByServerId(itemId);
    }

    auto brush = std::make_unique<RawBrush>(itemId, itemType);
    auto ptr = brush.get();
    raw_brushes_[itemId] = std::move(brush);
    const auto brushId = nextBrushId_++;
    brush_ids_[ptr] = brushId;
    brushes_by_id_[brushId] = ptr;
    brush_ids_by_name_[normalizeKey(ptr->getName())] = brushId;
    registerItemBinding(itemId, ptr);
    return ptr;
}

void BrushRegistry::setClientDataService(Services::ClientDataService* clientData) {
    clientData_ = clientData;

    for (auto& [itemId, brush] : raw_brushes_) {
        if (auto* rawBrush = dynamic_cast<RawBrush*>(brush.get())) {
            rawBrush->setCachedType(clientData_ ? clientData_->getItemTypeByServerId(itemId) : nullptr);
        }
    }
}

void BrushRegistry::registerItemBinding(uint16_t itemId, IBrush* brush) {
    if (!brush || itemId == 0) {
        return;
    }

    auto &allBindings = item_bindings_all_[itemId];
    if (std::find(allBindings.begin(), allBindings.end(), brush) == allBindings.end()) {
        allBindings.push_back(brush);
    }

    auto it = item_bindings_.find(itemId);
    if (it == item_bindings_.end() || it->second == nullptr ||
        it->second->getType() == BrushType::Raw ||
        brush->getType() != BrushType::Raw) {
        item_bindings_[itemId] = brush;
    }
}

void BrushRegistry::registerCreatureBinding(const std::string& creatureName, IBrush* brush) {
    if (!brush || creatureName.empty()) {
        return;
    }
    creature_bindings_[normalizeKey(creatureName)] = brush;
}

IBrush* BrushRegistry::getBrushForItem(uint16_t itemId) const {
    auto it = item_bindings_.find(itemId);
    if (it != item_bindings_.end()) {
        return it->second;
    }

    auto rawIt = raw_brushes_.find(itemId);
    return rawIt != raw_brushes_.end() ? rawIt->second.get() : nullptr;
}

std::vector<IBrush*> BrushRegistry::getBrushesForItem(uint16_t itemId) const {
    if (auto it = item_bindings_all_.find(itemId); it != item_bindings_all_.end()) {
        return it->second;
    }

    if (auto rawIt = raw_brushes_.find(itemId); rawIt != raw_brushes_.end()) {
        return {rawIt->second.get()};
    }

    return {};
}

IBrush* BrushRegistry::getBrushForCreature(const std::string& creatureName) const {
    auto it = creature_bindings_.find(normalizeKey(creatureName));
    return it != creature_bindings_.end() ? it->second : nullptr;
}

IBrush *BrushRegistry::getOrCreatePlaceholderBrush(const std::string &name) {
    if (name.empty()) {
        return nullptr;
    }

    if (auto *existingBrush = getBrush(name);
        existingBrush && existingBrush->getType() == BrushType::Placeholder) {
        return existingBrush;
    }

    const auto normalizedName = normalizeKey(name);
    if (const auto it = placeholder_brushes_.find(normalizedName);
        it != placeholder_brushes_.end()) {
        return it->second.get();
    }

    auto placeholder = std::make_unique<PlaceholderBrush>(name);
    auto *placeholderPtr = placeholder.get();
    placeholder_brushes_[normalizedName] = std::move(placeholder);
    return placeholderPtr;
}

IBrush* BrushRegistry::resolveBrushForTile(const Domain::Tile& tile) const {
    auto pickBestItemBrush = [this](const Domain::Tile& candidateTile) -> IBrush* {
        for (size_t index = candidateTile.getItemCount(); index > 0; --index) {
            const auto* item = candidateTile.getItem(index - 1);
            if (!item) {
                continue;
            }

            if (item->getOwnerBrushId() != InvalidBrushId) {
                if (auto* brush = getBrushById(item->getOwnerBrushId())) {
                    return brush;
                }
            }

            for (auto* brush : getBrushesForItem(item->getServerId())) {
                if (!brush || brush->getType() == BrushType::Raw) {
                    continue;
                }
                return brush;
            }
        }

        for (size_t index = candidateTile.getItemCount(); index > 0; --index) {
            const auto* item = candidateTile.getItem(index - 1);
            if (!item) {
                continue;
            }

            if (auto* brush = getBrushForItem(item->getServerId())) {
                return brush;
            }
        }

        return nullptr;
    };

    if (const auto* creature = tile.getCreature(); creature && tile.getCreatureBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getCreatureBrushId())) {
            return brush;
        }
    }

    if (tile.getSpawnBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getSpawnBrushId())) {
            return brush;
        }
    }

    if (tile.getWaypointBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getWaypointBrushId())) {
            return brush;
        }
    }

    if (tile.getHouseExitBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getHouseExitBrushId())) {
            return brush;
        }
    }

    if (tile.getHouseBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getHouseBrushId())) {
            return brush;
        }
    }

    if (tile.getOptionalBorderBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(tile.getOptionalBorderBrushId())) {
            return brush;
        }
    }

    for (const auto flag : {Domain::TileFlag::ProtectionZone, Domain::TileFlag::NoPvp,
                            Domain::TileFlag::NoLogout, Domain::TileFlag::PvpZone,
                            Domain::TileFlag::Refresh}) {
        if (auto brushId = tile.getZoneBrushId(flag); brushId != InvalidBrushId) {
            if (auto* brush = getBrushById(brushId)) {
                return brush;
            }
        }
    }

    if (tile.hasCreature()) {
        if (const auto* creature = tile.getCreature()) {
            if (auto* brush = getBrushForCreature(creature->name)) {
                return brush;
            }
        }
    }

    if (auto* brush = pickBestItemBrush(tile)) {
        return brush;
    }

    if (const auto* ground = tile.getGround(); ground && ground->getOwnerBrushId() != InvalidBrushId) {
        if (auto* brush = getBrushById(ground->getOwnerBrushId())) {
            return brush;
        }
    }

    if (tile.hasCreature()) {
        if (const auto* creature = tile.getCreature()) {
            if (auto* brush = getBrushForCreature(creature->name)) {
                return brush;
            }
        }
    }

    if (const auto* ground = tile.getGround()) {
        if (auto* brush = getBrushForItem(ground->getServerId())) {
            return brush;
        }
    }

    return nullptr;
}

BrushId BrushRegistry::getBrushId(const IBrush* brush) const {
    if (!brush) {
        return InvalidBrushId;
    }

    if (const auto it = brush_ids_.find(brush); it != brush_ids_.end()) {
        return it->second;
    }
    return InvalidBrushId;
}

IBrush* BrushRegistry::getBrushById(BrushId brushId) const {
    if (brushId == InvalidBrushId) {
        return nullptr;
    }

    if (const auto it = brushes_by_id_.find(brushId); it != brushes_by_id_.end()) {
        return it->second;
    }
    return nullptr;
}

std::unique_ptr<Domain::Item> BrushRegistry::createItem(uint16_t itemId,
                                                        uint16_t subtype) const {
    auto item = std::make_unique<Domain::Item>(itemId, subtype);
    if (clientData_) {
        if (const auto* itemType = clientData_->getItemTypeByServerId(itemId)) {
            item->setType(itemType);
            item->setClientId(itemType->client_id);
        }
    }
    return item;
}

void BrushRegistry::registerBorderTemplate(uint32_t id, BorderBlock border) {
    if (id == 0) {
        return;
    }

    for (size_t index = 0; index < BorderBlock::kEdgeTypeCount; ++index) {
        const auto edge = static_cast<EdgeType>(index);
        if (!border.hasItemsFor(edge)) {
            continue;
        }

        for (const auto &[itemId, _] : border.getItems(edge)) {
            if (itemId == 0 || itemId > std::numeric_limits<uint16_t>::max()) {
                continue;
            }

            border_item_metadata_.try_emplace(
                static_cast<uint16_t>(itemId),
                BorderItemMetadata{.group = border.getGroup(), .alignment = edge});
        }
    }

    border_templates_[id] = std::move(border);
}

const BorderBlock* BrushRegistry::getBorderTemplate(uint32_t id) const {
    auto it = border_templates_.find(id);
    return it != border_templates_.end() ? &it->second : nullptr;
}

const BrushRegistry::BorderItemMetadata *
BrushRegistry::getBorderItemMetadata(uint16_t itemId) const {
    if (const auto it = border_item_metadata_.find(itemId);
        it != border_item_metadata_.end()) {
        return &it->second;
    }
    return nullptr;
}

void BrushRegistry::clear() {
    named_brushes_.clear();
    raw_brushes_.clear();
    placeholder_brushes_.clear();
    external_brushes_.clear();
    item_bindings_.clear();
    item_bindings_all_.clear();
    creature_bindings_.clear();
    border_templates_.clear();
    border_item_metadata_.clear();
    brush_ids_.clear();
    brushes_by_id_.clear();
    brush_ids_by_name_.clear();
    nextBrushId_ = 1;
}

std::vector<IBrush*> BrushRegistry::getAllBrushes() const {
    std::vector<IBrush*> result;
    result.reserve(named_brushes_.size() + raw_brushes_.size() + external_brushes_.size());

    for (const auto& [_, brush] : named_brushes_) {
        result.push_back(brush.get());
    }
    for (const auto& [_, brush] : raw_brushes_) {
        result.push_back(brush.get());
    }
    for (auto *brush : external_brushes_) {
        if (brush) {
            result.push_back(brush);
        }
    }
    return result;
}

} // namespace MapEditor::Brushes
