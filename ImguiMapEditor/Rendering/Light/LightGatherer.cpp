#include "LightGatherer.h"
#include "Services/ClientDataService.h"
#include "Domain/Tile.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

void LightGatherer::clear() {
    lights_.clear();
}

void LightGatherer::gatherForChunk(
    const Domain::ChunkedMap& map,
    int32_t chunk_x, int32_t chunk_y,
    Services::ClientDataService* client_data,
    int16_t floor)
{
    if (!client_data) return;
    
    // We need to check the target chunk AND its 8 neighbors (3x3 grid)
    // because a light in a neighbor might spill into this chunk.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            // Get visible chunks directly from the map using exact chunk coordinates
            // Note: ChunkedMap doesn't expose "getChunk(cx, cy)" publicly in the interface I saw earlier,
            // but it has `getVisibleChunks` which takes pixel/tile coords.
            // Let's use tile coordinates to find the chunks.
            
            int32_t const target_cx = chunk_x + dx;
            int32_t const target_cy = chunk_y + dy;
            
            // Calculate world tile coordinates for this chunk
            int32_t const tile_start_x = target_cx * Domain::Chunk::SIZE;
            int32_t const tile_start_y = target_cy * Domain::Chunk::SIZE;
            int32_t const tile_end_x = tile_start_x + Domain::Chunk::SIZE;
            int32_t const tile_end_y = tile_start_y + Domain::Chunk::SIZE;
            
            // Efficiently get the single chunk for this region (or just iterate tiles if API limits)
            // The existing `getVisibleChunks` returns a vector. Let's use that.
            std::vector<Domain::Chunk*> chunks;
            map.getVisibleChunks(tile_start_x, tile_start_y, tile_end_x - 1, tile_end_y - 1, floor, chunks);
            
            for (Domain::Chunk const* chunk : chunks) {
                // Ensure we are processing the correct chunk (getVisibleChunks might return overlaps if not careful, 
                // but with exact coordinates it should be 1 chunk)
                if (!chunk) continue;
                
                chunk->forEachTile([&](const Domain::Tile* tile) {
                    if (!tile || tile->getZ() != floor) return;
                    
                    int32_t const tile_x = tile->getX();
                    int32_t const tile_y = tile->getY();
                    
                    // Check ground item for light
                    const Domain::Item* ground = tile->getGround();
                    if (ground) {
                        const Domain::ItemType* item_type = client_data->getItemTypeByServerId(ground->getServerId());
                        if (item_type && item_type->light_level > 0) {
                            Domain::LightSource light;
                            light.x = tile_x;
                            light.y = tile_y;
                            light.color = item_type->light_color;
                            light.intensity = item_type->light_level;
                            lights_.push_back(light);
                            
                            // Check for invalid color
                            if (light.color == 0) {
                                // spdlog::warn("Light source with NO COLOR (Index 0) found at {},{}: ItemID {}, Level {}", tile_x, tile_y, ground->getServerId(), light.intensity);
                            } else {
                                // spdlog::debug("Light found at {},{}: ItemID {}, Level {}, Color {}", tile_x, tile_y, ground->getServerId(), light.intensity, light.color);
                            }
                        }
                    }
                    
                    // Check all items on the tile
                    for (const auto& item_ptr : tile->getItems()) {
                        if (!item_ptr) continue;
                        const Domain::ItemType* item_type = client_data->getItemTypeByServerId(item_ptr->getServerId());
                        if (item_type && item_type->light_level > 0) {
                            Domain::LightSource light;
                            light.x = tile_x;
                            light.y = tile_y;
                            light.color = item_type->light_color;
                            light.intensity = item_type->light_level;
                            lights_.push_back(light);
                            
                            if (light.color == 0) {
                                // spdlog::warn("Light source with NO COLOR (Index 0) found at {},{}: ItemID {}, Level {}", tile_x, tile_y, item_ptr->getServerId(), light.intensity);
                            } else {
                                // spdlog::debug("Light found at {},{}: ItemID {}, Level {}, Color {}", tile_x, tile_y, item_ptr->getServerId(), light.intensity, light.color);
                            }
                        }
                    }
                });
            }
        }
    }
}

void LightGatherer::gatherForChunkMultiFloor(
    const Domain::ChunkedMap& map,
    int32_t chunk_x, int32_t chunk_y,
    Services::ClientDataService* client_data,
    int16_t start_floor,
    int16_t end_floor)
{
    if (!client_data) return;
    
    constexpr int GROUND_LAYER = 7;
    
    // Iterate through all floors in range (from start_floor down to end_floor)
    for (int16_t floor = start_floor; floor >= end_floor; --floor) {
        
        // Calculate isometric offset for this floor (RME-style)
        // Lights from higher floors (lower Z) appear shifted in X and Y
        int32_t floor_offset = 0;
        if (floor <= GROUND_LAYER) {
            // Above ground: offset based on distance from ground layer
            floor_offset = GROUND_LAYER - floor;
        }
        // Underground floors don't get offset in RME's light system
        
        // We need to check the target chunk AND its 8 neighbors (3x3 grid)
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                gatherLightsFromNeighborChunk(
                    map, chunk_x + dx, chunk_y + dy, 
                    client_data, floor, floor_offset);
            }
        }
    }
}

void LightGatherer::gatherLightsFromNeighborChunk(
    const Domain::ChunkedMap& map,
    int32_t target_cx, int32_t target_cy,
    Services::ClientDataService* client_data,
    int16_t floor, int32_t floor_offset)
{
    int32_t const tile_start_x = target_cx * Domain::Chunk::SIZE;
    int32_t const tile_start_y = target_cy * Domain::Chunk::SIZE;
    int32_t const tile_end_x = tile_start_x + Domain::Chunk::SIZE;
    int32_t const tile_end_y = tile_start_y + Domain::Chunk::SIZE;
    
    std::vector<Domain::Chunk*> chunks;
    map.getVisibleChunks(tile_start_x, tile_start_y, tile_end_x - 1, tile_end_y - 1, floor, chunks);
    
    for (Domain::Chunk const* chunk : chunks) {
        if (!chunk) continue;
        
        chunk->forEachTile([&](const Domain::Tile* tile) {
            if (!tile || tile->getZ() != floor) return;
            
            int32_t const tile_x = tile->getX();
            int32_t const tile_y = tile->getY();
            
            // Apply isometric offset (RME-style: lights from higher floors
            // are projected onto 2D with offset)
            int32_t adjusted_x = tile_x - floor_offset;
            int32_t adjusted_y = tile_y - floor_offset;
            
            // Helper lambda to add light
            auto addLightFromItem = [&](const Domain::Item* item) {
                if (!item) return;
                const Domain::ItemType* item_type = client_data->getItemTypeByServerId(item->getServerId());
                if (item_type && item_type->light_level > 0) {
                    lights_.emplace_back(Domain::LightSource{
                        .x = adjusted_x,
                        .y = adjusted_y,
                        .color = item_type->light_color,
                        .intensity = item_type->light_level
                    });
                }
            };
            
            // Check ground item
            addLightFromItem(tile->getGround());
            
            // Check all items on the tile
            for (const auto& item_ptr : tile->getItems()) {
                addLightFromItem(item_ptr.get());
            }
        });
    }
}

} // namespace Rendering
} // namespace MapEditor
