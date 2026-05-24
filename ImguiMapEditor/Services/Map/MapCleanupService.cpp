#include "MapCleanupService.h"
#include "Domain/Tile.h"
#include "Domain/Item.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

CleanupResult MapCleanupService::cleanInvalidItems(
    Domain::ChunkedMap& map,
    const ClientDataService& client_data,
    ProgressCallback on_progress)
{
    CleanupResult result;
    result.total_tiles = map.getTileCount();
    
    if (result.total_tiles == 0) {
        return result;
    }
    
    // We need a mutable iteration since we're modifying tiles
    // ChunkedMap::forEachTile only provides const access to tiles
    // We'll iterate floor by floor using getVisibleChunks on full range
    
    for (int16_t z = Domain::ChunkedMap::FLOOR_MIN; z <= Domain::ChunkedMap::FLOOR_MAX; ++z) {
        map.forEachTileOnFloorMutable(z, [&](Domain::Tile* tile) {
            if (!tile) return;
            
            result.tiles_processed++;
            
            // Check ground item
            if (tile->hasGround()) {
                Domain::Item const* ground = tile->getGround();
                if (ground && !client_data.getItemTypeByServerId(ground->getServerId())) {
                    tile->removeGround();
                    result.items_removed++;
                }
            }
            
            // Check stacked items - iterate backwards to safely remove
            const auto& items = tile->getItems();
            for (size_t i = items.size(); i > 0; --i) {
                size_t const idx = i - 1;
                if (items[idx] && !client_data.getItemTypeByServerId(items[idx]->getServerId())) {
                    tile->removeItem(idx);
                    result.items_removed++;
                }
            }
            
            // Progress callback
            if (on_progress && result.tiles_processed % 10000 == 0) {
                float const progress = static_cast<float>(result.tiles_processed) / 
                                 static_cast<float>(result.total_tiles);
                on_progress(progress);
            }
        });
    }
    
    if (on_progress) {
        on_progress(1.0f);
    }
    
    return result;
}

CleanupResult MapCleanupService::cleanHouseItems(
    Domain::ChunkedMap& map,
    const ClientDataService& client_data,
    ProgressCallback on_progress)
{
    CleanupResult result;
    result.total_tiles = map.getTileCount();
    
    if (result.total_tiles == 0) {
        return result;
    }
    
    for (int16_t z = Domain::ChunkedMap::FLOOR_MIN; z <= Domain::ChunkedMap::FLOOR_MAX; ++z) {
        map.forEachTileOnFloorMutable(z, [&](Domain::Tile* tile) {
            if (!tile) return;
            
            result.tiles_processed++;
            
            // Only process house tiles
            if (!tile->isHouseTile()) {
                return;
            }
            
            // Check stacked items - iterate backwards to safely remove
            const auto& items = tile->getItems();
            for (size_t i = items.size(); i > 0; --i) {
                size_t const idx = i - 1;
                const Domain::Item* item = items[idx].get();
                if (!item) continue;
                
                // Get item type to check if moveable
                const Domain::ItemType* item_type = client_data.getItemTypeByServerId(item->getServerId());
                if (item_type && item_type->is_moveable) {
                    tile->removeItem(idx);
                    result.items_removed++;
                }
            }
            
            // Progress callback
            if (on_progress && result.tiles_processed % 10000 == 0) {
                float const progress = static_cast<float>(result.tiles_processed) / 
                                 static_cast<float>(result.total_tiles);
                on_progress(progress);
            }
        });
    }
    
    if (on_progress) {
        on_progress(1.0f);
    }
    
    return result;
}

CleanupResult MapCleanupService::removeItemsById(
    Domain::ChunkedMap& map,
    uint16_t item_id,
    ProgressCallback on_progress)
{
    CleanupResult result;
    result.total_tiles = map.getTileCount();
    
    if (result.total_tiles == 0) {
        return result;
    }
    
    for (int16_t z = Domain::ChunkedMap::FLOOR_MIN; z <= Domain::ChunkedMap::FLOOR_MAX; ++z) {
        map.forEachTileOnFloorMutable(z, [&](Domain::Tile* tile) {
            if (!tile) return;
            
            result.tiles_processed++;
            
            // Check ground item
            if (tile->hasGround()) {
                Domain::Item const* ground = tile->getGround();
                if (ground && ground->getServerId() == item_id) {
                    tile->removeGround();
                    result.items_removed++;
                }
            }
            
            // Check stacked items - iterate backwards to safely remove
            const auto& items = tile->getItems();
            for (size_t i = items.size(); i > 0; --i) {
                size_t const idx = i - 1;
                if (items[idx] && items[idx]->getServerId() == item_id) {
                    tile->removeItem(idx);
                    result.items_removed++;
                }
            }
            
            // Progress callback
            if (on_progress && result.tiles_processed % 10000 == 0) {
                float const progress = static_cast<float>(result.tiles_processed) / 
                                 static_cast<float>(result.total_tiles);
                on_progress(progress);
            }
        });
    }
    
    if (on_progress) {
        on_progress(1.0f);
    }
    
    return result;
}

} // namespace Services
} // namespace MapEditor
