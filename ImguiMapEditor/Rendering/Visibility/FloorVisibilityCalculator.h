#pragma once
#include "Domain/ChunkedMap.h"
#include "Domain/ItemType.h"
#include "Services/ClientDataService.h"
#include <cstdint>

namespace MapEditor {
namespace Rendering {

/**
 * Floor visibility constants matching OTClient
 */
struct FloorConstants {
    static constexpr int SEA_FLOOR = 7;
    static constexpr int MAX_Z = 15;
    static constexpr int UNDERGROUND_FLOOR = 8;
    static constexpr int AWARE_UNDERGROUND_FLOOR_RANGE = 2;
};

/**
 * Calculates floor visibility for client-accurate rendering.
 * 
 * This class implements OTClient's floor visibility algorithm:
 * - On surface (Z <= 7): Sees floors 0-7, with upper floors hidden by roofs
 * - Underground (Z > 7): Sees only Z ± 2 floors
 * 
 * Floor visibility is determined by checking tiles for blocking properties:
 * - Ground tiles block view unless translucent
 * - isOnBottom items (walls) block view unless isDontHide
 * - Windows/doors (isLookPossible) allow viewing diagonal tiles
 */
class FloorVisibilityCalculator {
public:
    explicit FloorVisibilityCalculator(Services::ClientDataService* client_data);
    
    /**
     * Calculate first (topmost) visible floor from camera position.
     * 
     * @param map The map to check tiles from
     * @param camera_x Camera X position
     * @param camera_y Camera Y position
     * @param camera_z Camera Z position (floor)
     * @return First visible floor (0-15)
     */
    int calcFirstVisibleFloor(
        const Domain::ChunkedMap& map,
        int camera_x, int camera_y, int camera_z) const;
    
    /**
     * Calculate last (deepest) visible floor from camera position.
     * 
     * @param camera_z Camera Z position (floor)
     * @return Last visible floor (0-15)
     */
    int calcLastVisibleFloor(int camera_z);

    
    /**
     * Check if a tile limits the view of floors above it.
     * 
     * @param tile Tile to check
     * @param is_free_view True for free camera, false for player-attached view
     * @return True if tile blocks view of upper floors
     */
    bool tileLimitsFloorsView(const Domain::Tile* tile, bool is_free_view) const;
    
    /**
     * Check if a tile allows looking through (windows, doors).
     * 
     * @param tile Tile to check
     * @return True if tile is transparent to view
     */
    bool isLookPossible(const Domain::Tile* tile) const;

private:
    Services::ClientDataService* client_data_;
    
    /**
     * Get ItemType for an item's client ID.
     */
    const Domain::ItemType* getItemType(const Domain::Item* item) const;
};

} // namespace Rendering
} // namespace MapEditor
