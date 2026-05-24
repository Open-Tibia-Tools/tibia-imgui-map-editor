#pragma once
#include "Domain/Spawn.h"
#include "Domain/Creature.h"
#include "Domain/Position.h"
#include "Domain/ChunkedMap.h"
#include "Core/Config.h"
#include <unordered_map>
#include <string>
#include <cstdint>
#include <random>

namespace MapEditor {
namespace Services {

/**
 * Per-creature animation state for walk simulation.
 */
struct CreatureAnimState {
    Domain::Position current_pos;     // Current tile position (after move)
    Domain::Position spawn_center;    // Original spawn center
    int32_t original_offset_x = 0;    // Original spawn offset X
    int32_t original_offset_y = 0;    // Original spawn offset Y
    int32_t spawn_radius = 0;         // Movement constraint
    uint8_t direction = 2;            // Current facing (0=N, 1=E, 2=S, 3=W)
    int animation_frame = 0;          // Current walk frame (0-N)
    float move_timer = 0.0f;          // Time until next move attempt
    float walk_progress = 0.0f;       // Walk progress 0.0 to 1.0
    bool is_walking = false;          // Currently in walk animation
    
    // Smooth walk offset in pixels (for rendering)
    // When walk starts: offset = -direction * 32 (start from previous tile visually)
    // As walk progresses: offset lerps to 0
    float walk_offset_x = 0.0f;       // Pixel offset X
    float walk_offset_y = 0.0f;       // Pixel offset Y
    
    std::string creature_name;        // For outfit lookup
};

/**
 * Manages creature walk simulation for visual feedback.
 * 
 * Only simulates creatures currently visible in viewport.
 * Separate concern from rendering - just manages animation state.
 */
class CreatureSimulator {
public:
    CreatureSimulator();
    
    /**
     * Update all creature states for current frame.
     * Only updates creatures within the given viewport bounds.
     * 
     * @param delta_time Frame time in seconds
     * @param viewport_min Top-left tile of visible area
     * @param viewport_max Bottom-right tile of visible area  
     * @param current_floor Only simulate this floor
     * @param map Map for walkability checks (optional)
     * @param client_data Client data for item blocking flags (optional)
     */
    void update(float delta_time,
                const Domain::Position& viewport_min,
                const Domain::Position& viewport_max,
                int current_floor,
                Domain::ChunkedMap* map = nullptr,
                class ClientDataService* client_data = nullptr);
    
    /**
     * Get animation state for a creature on a tile.
     * Creates state lazily if not exists.
     * 
     * @param creature Creature data (per-tile)
     * @param position Tile position of the creature
     * @param map Map pointer for spawn radius lookup
     * @return Pointer to state, or nullptr if simulation disabled
     */
    CreatureAnimState* getOrCreateState(const Domain::Creature* creature,
                                         const Domain::Position& position,
                                         Domain::ChunkedMap* map = nullptr);
    
    /**
     * Get state without creating (for read-only access).
     */
    static const CreatureAnimState* getState(const Domain::Position& spawn_center,
                                       const std::string& creature_name) ;
    
    /**
     * Clear all animation states.
     * Call when map changes or simulation toggled off.
     */
    void reset();
    
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    // Attempt to move a creature one tile in a random direction
    void tryMoveCreature(CreatureAnimState& state,
                        Domain::ChunkedMap* map,
                        class ClientDataService* client_data);
    
    // Generate unique key for creature at position
    static uint64_t makeKey(const Domain::Creature* creature) ;
    
    bool enabled_ = false;
    std::unordered_map<uint64_t, CreatureAnimState> states_;

    // Optimization: Spatial index for O(1) collision checks
    // Rebuilt every frame in update()
    // Map of Position -> Count (to handle stacked creatures correctly)
    std::unordered_map<Domain::Position, int> occupied_positions_;

    std::mt19937 rng_;

    /// Reusable distributions to avoid reconstruction in hot loops
    std::uniform_real_distribution<float> chance_dist_; // 0.0f to 1.0f
    std::uniform_real_distribution<float> interval_dist_; // For random move intervals
    std::uniform_int_distribution<int> direction_dist_; // 0 to NUM_DIRECTIONS - 1

    static constexpr int NUM_DIRECTIONS = 4;
};

} // namespace Services
} // namespace MapEditor
