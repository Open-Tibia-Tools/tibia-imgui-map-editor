#include "Rendering/Visibility/FloorVisibilityCalculator.h"
#include <algorithm>
#include <cmath>

namespace MapEditor {
namespace Rendering {

FloorVisibilityCalculator::FloorVisibilityCalculator(Services::ClientDataService* client_data)
    : client_data_(client_data)
{
}

const Domain::ItemType* FloorVisibilityCalculator::getItemType(const Domain::Item* item) const {
    if (!item || !client_data_) return nullptr;
    return client_data_->getItemTypeByServerId(item->getServerId());
}

bool FloorVisibilityCalculator::tileLimitsFloorsView(const Domain::Tile* tile, bool is_free_view) const {
    if (!tile) return false;
    
    // Check ground first
    const Domain::Item* ground = tile->getGround();
    if (!ground) return false;
    
    const Domain::ItemType* ground_type = getItemType(ground);
    if (!ground_type) return false;
    
    // Items with isDontHide never block view
    if (ground_type->is_dont_hide) return false;
    
    // Ground tiles block view
    if (ground_type->is_ground) return true;
    
    // isOnBottom items (walls) block view
    if (ground_type->is_on_bottom) {
        if (is_free_view) {
            // Free view: any wall blocks
            return true;
        } else {
            // Player view: only walls that block projectiles
            return ground_type->blocks_projectile;
        }
    }
    
    // Check other items on tile for blocking
    for (const auto& item : tile->getItems()) {
        const Domain::ItemType* item_type = getItemType(item.get());
        if (!item_type) continue;
        
        if (item_type->is_dont_hide) continue;
        
        if (item_type->is_ground) return true;
        
        if (item_type->is_on_bottom) {
            if (is_free_view) {
                return true;
            } else {
                if (item_type->blocks_projectile) return true;
            }
        }
    }
    
    return false;
}

bool FloorVisibilityCalculator::isLookPossible(const Domain::Tile* tile) const {
    if (!tile) return true;  // Empty tile is transparent
    
    // Check all items for projectile blocking
    const Domain::Item* ground = tile->getGround();
    if (ground) {
        const Domain::ItemType* ground_type = getItemType(ground);
        if (ground_type && ground_type->blocks_projectile) {
            return false;
        }
    }
    
    for (const auto& item : tile->getItems()) {
        const Domain::ItemType* item_type = getItemType(item.get());
        if (item_type && item_type->blocks_projectile) {
            return false;
        }
    }
    
    return true;
}

int FloorVisibilityCalculator::calcFirstVisibleFloor(
    const Domain::ChunkedMap& map,
    int camera_x, int camera_y, int camera_z) const
{
    using FC = FloorConstants;
    
    int first_floor = 0;
    
    // Underground: limit view to underground floors only
    if (camera_z > FC::SEA_FLOOR) {
        first_floor = std::max(camera_z - FC::AWARE_UNDERGROUND_FLOOR_RANGE, 
                               static_cast<int>(FC::UNDERGROUND_FLOOR));
    }
    
    // Check 3x3 area around camera for blocking tiles
    for (int ix = -1; ix <= 1 && first_floor < camera_z; ++ix) {
        for (int iy = -1; iy <= 1 && first_floor < camera_z; ++iy) {
            int const pos_x = camera_x + ix;
            int const pos_y = camera_y + iy;
            
            // Center tile OR diagonal tiles where we can look through
            bool const is_center = (ix == 0 && iy == 0);
            bool const is_diagonal = (std::abs(ix) == std::abs(iy)) && !is_center;
            
            // For diagonal tiles, check if we can look through (window/door)
            if (!is_center && is_diagonal) {
                const Domain::Tile* current_tile = map.getTile(pos_x, pos_y, camera_z);
                if (!isLookPossible(current_tile)) {
                    continue;  // Can't look diagonally through solid tiles
                }
            }
            
            // Walk up through floors checking for blockers
            // OTClient uses coveredUp which shifts x+1, y+1, z-1
            int const check_x = pos_x;
            int const check_y = pos_y;
            
            for (int check_z = camera_z - 1; check_z >= first_floor; --check_z) {
                // Apply covered position shift (each floor up = x+1, y+1)
                int const z_diff = camera_z - check_z;
                int const covered_x = pos_x + z_diff;
                int const covered_y = pos_y + z_diff;
                
                // Check tile directly above (physical position)
                const Domain::Tile* upper_tile = map.getTile(pos_x, pos_y, check_z);
                bool const can_look = isLookPossible(map.getTile(pos_x, pos_y, camera_z));
                
                if (upper_tile && tileLimitsFloorsView(upper_tile, !can_look)) {
                    first_floor = check_z + 1;
                    break;
                }
                
                // Check tile geometrically above (covered position)
                const Domain::Tile* covered_tile = map.getTile(covered_x, covered_y, check_z);
                if (covered_tile && tileLimitsFloorsView(covered_tile, can_look)) {
                    first_floor = check_z + 1;
                    break;
                }
            }
        }
    }
    
    return std::clamp(first_floor, 0, static_cast<int>(FC::MAX_Z));
}

int FloorVisibilityCalculator::calcLastVisibleFloor(int camera_z) {
    using FC = FloorConstants;
    
    int z;
    
    if (camera_z > FC::SEA_FLOOR) {
        // Underground: see current floor + range below
        z = camera_z + FC::AWARE_UNDERGROUND_FLOOR_RANGE;
    } else {
        // Surface: see down to sea floor
        z = FC::SEA_FLOOR;
    }
    
    return std::clamp(z, 0, static_cast<int>(FC::MAX_Z));
}

} // namespace Rendering
} // namespace MapEditor
