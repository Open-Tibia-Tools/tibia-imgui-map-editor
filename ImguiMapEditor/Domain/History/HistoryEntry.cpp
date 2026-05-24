#include "HistoryEntry.h"
#include "../Item.h"
#include "Services/ClientDataService.h"
#include "Services/Selection/SelectionService.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Domain::History {

HistoryEntry::HistoryEntry(const std::string& description, ActionType type)
    : description_(description)
    , type_(type) {
}

void HistoryEntry::addBeforeSnapshot(TileSnapshot snapshot) {
    beforeSnapshots_.push_back(std::move(snapshot));
}

void HistoryEntry::addAfterSnapshot(TileSnapshot snapshot) {
    afterSnapshots_.push_back(std::move(snapshot));
}

void HistoryEntry::compress(bool enable) {
    if (!enable || compressed_) return;
    
    // Compress BEFORE snapshots
    beforeOriginalSizes_.reserve(beforeSnapshots_.size());
    for (auto& snapshot : beforeSnapshots_) {
        size_t const original_size = snapshot.data().size();
        beforeOriginalSizes_.push_back(original_size);
        
        if (original_size > 64) {  // Only compress if worth it
            auto compressed = TileSnapshotCodec::compress(snapshot.data());
            snapshot.setData(std::move(compressed));
        }
    }
    
    // Compress AFTER snapshots
    afterOriginalSizes_.reserve(afterSnapshots_.size());
    for (auto& snapshot : afterSnapshots_) {
        size_t const original_size = snapshot.data().size();
        afterOriginalSizes_.push_back(original_size);
        
        if (original_size > 64) {
            auto compressed = TileSnapshotCodec::compress(snapshot.data());
            snapshot.setData(std::move(compressed));
        }
    }
    
    compressed_ = true;
}

void HistoryEntry::applySnapshots(ChunkedMap* map, 
                                   const std::vector<TileSnapshot>& snapshots,
                                   const std::vector<size_t>& originalSizes,
                                   Services::ClientDataService* clientData) const {
    for (size_t i = 0; i < snapshots.size(); ++i) {
        TileSnapshot snapshot = snapshots[i];  // Copy for decompression
        
        // Decompress if needed
        if (compressed_ && i < originalSizes.size() && originalSizes[i] > 64) {
            auto decompressed = TileSnapshotCodec::decompress(
                snapshot.data(), originalSizes[i]);
            snapshot.setData(std::move(decompressed));
        }
        
        // Restore tile
        auto tile = snapshot.restore();
        const Position& pos = snapshot.getPosition();
        
        if (tile) {
            // CRITICAL: Resolve ItemTypes for all items in this tile
            // Without this, items won't render and ground detection fails
            if (clientData) {
                // Resolve ground item type
                if (tile->hasGround()) {
                    Item* ground = tile->getGround();
                    if (ground && !ground->getType()) {
                        const ItemType* type = clientData->getItemTypeByServerId(ground->getServerId());
                        ground->setType(type);
                    }
                }
                
                // Resolve stacked items types using getItem(index) for non-const access
                for (size_t j = 0; j < tile->getItemCount(); ++j) {
                    Item* item = tile->getItem(j);
                    if (item && !item->getType()) {
                        const ItemType* type = clientData->getItemTypeByServerId(item->getServerId());
                        item->setType(type);
                    }
                }
            }
            
            // Replace tile in map
            map->setTile(pos, std::move(tile));
        } else {
            // Remove tile (snapshot was empty)
            map->removeTile(pos);
        }
    }
}

void HistoryEntry::undo(ChunkedMap* map, Services::ClientDataService* clientData,
                        Services::Selection::SelectionService* selection) {
    // Restore tile states
    applySnapshots(map, beforeSnapshots_, beforeOriginalSizes_, clientData);
    
    // Restore selection state if captured
    if (selection && selection_before_.has_value()) {
        selection->restoreSnapshot(*selection_before_);
    }
}

void HistoryEntry::redo(ChunkedMap* map, Services::ClientDataService* clientData,
                        Services::Selection::SelectionService* selection) {
    // Restore tile states
    applySnapshots(map, afterSnapshots_, afterOriginalSizes_, clientData);
    
    // Restore selection state if captured
    if (selection && selection_after_.has_value()) {
        selection->restoreSnapshot(*selection_after_);
    }
}

size_t HistoryEntry::memsize() const {
    size_t size = sizeof(*this);
    size += description_.capacity();
    
    for (const auto& s : beforeSnapshots_) {
        size += s.memsize();
    }
    for (const auto& s : afterSnapshots_) {
        size += s.memsize();
    }
    
    size += beforeOriginalSizes_.capacity() * sizeof(size_t);
    size += afterOriginalSizes_.capacity() * sizeof(size_t);
    
    return size;
}

} // namespace MapEditor::Domain::History
