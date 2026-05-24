#include "CreatureSimulator.h"
#include "ClientDataService.h"
#include "Domain/ItemType.h"
#include "Core/Config.h"
#include <cmath>

namespace MapEditor {
namespace Services {

// Use centralized Config values
using namespace Config::Simulation;

CreatureSimulator::CreatureSimulator() 
    : rng_(std::random_device{}()),
      chance_dist_(0.0f, 1.0f),
      interval_dist_(Config::Simulation::RANDOM_MOVE_INTERVAL_MIN, Config::Simulation::RANDOM_MOVE_INTERVAL_MAX),
      direction_dist_(0, NUM_DIRECTIONS - 1) {
}

void CreatureSimulator::update(float delta_time,
                               const Domain::Position& viewport_min,
                               const Domain::Position& viewport_max,
                               int current_floor,
                               Domain::ChunkedMap* map,
                               ClientDataService* client_data) {
    if (!enabled_) return;
    
    // Performance: Rebuild spatial index once per frame
    occupied_positions_.clear();
    // Pre-allocate to avoid rehashes (approx size matches active creatures)
    occupied_positions_.reserve(states_.size());

    for (const auto& [key, state] : states_) {
        // Only track creatures on current floor for collision
        if (state.current_pos.z == current_floor) {
            occupied_positions_[state.current_pos]++;
        }
    }

    for (auto& [key, state] : states_) {
        // Skip if not on current floor
        if (state.current_pos.z != current_floor) continue;
        
        // Skip if outside viewport (with margin)
        int const margin = 2;
        if (state.current_pos.x < viewport_min.x - margin || state.current_pos.x > viewport_max.x + margin ||
            state.current_pos.y < viewport_min.y - margin || state.current_pos.y > viewport_max.y + margin) {
            continue;
        }
        
        if (state.is_walking) {
            // Update walk animation progress
            state.walk_progress += delta_time / WALK_DURATION_SEC;
            
            if (state.walk_progress >= 1.0f) {
                // Walk complete
                state.walk_progress = 1.0f;
                state.is_walking = false;
                state.walk_offset_x = 0.0f;
                state.walk_offset_y = 0.0f;
                state.animation_frame = 0;
            } else {
                // Interpolate walk offset (from 1.0 to 0.0)
                float const remaining = 1.0f - state.walk_progress;
                
                int dx = 0, dy = 0;
                switch (state.direction) {
                    case 0: dy = -1; break; // North
                    case 1: dx = 1; break;  // East
                    case 2: dy = 1; break;  // South
                    case 3: dx = -1; break; // West
                }
                
                // Creature walks FROM the offset toward current position
                state.walk_offset_x = -dx * remaining;
                state.walk_offset_y = -dy * remaining;
                
                // Animation frame (0-3)
                state.animation_frame = static_cast<int>(state.walk_progress * 4) % 4;
            }
        } else {
            // Countdown to next movement attempt
            state.move_timer -= delta_time;
            if (state.move_timer <= 0.0f) {
                state.move_timer = TICK_INTERVAL_SEC;
                
                // Random chance to move (33%) - Uses member distribution
                if (chance_dist_(rng_) < MOVE_CHANCE) {
                    tryMoveCreature(state, map, client_data);
                }
            }
        }
    }
}

void CreatureSimulator::tryMoveCreature(CreatureAnimState& state, 
                                        Domain::ChunkedMap* map,
                                        ClientDataService* client_data) {
    // Pick random direction (0=N, 1=E, 2=S, 3=W) - Uses member distribution
    int const new_dir = direction_dist_(rng_);
    
    int dx = 0, dy = 0;
    switch (new_dir) {
        case 0: dy = -1; break; // North
        case 1: dx = 1; break;  // East
        case 2: dy = 1; break;  // South
        case 3: dx = -1; break; // West
    }
    
    Domain::Position new_pos = state.current_pos;
    new_pos.x += dx;
    new_pos.y += dy;
    
    // Check radius constraint from SPAWN CENTER (not creature start)
    // spawn_center is the spawn tile position, spawn_radius is the spawn's radius
    int const dist_x = std::abs(new_pos.x - state.spawn_center.x);
    int const dist_y = std::abs(new_pos.y - state.spawn_center.y);
    if (dist_x > state.spawn_radius || dist_y > state.spawn_radius) {
        return; // Out of spawn area
    }
    
    // Check walkability - MUST have map to check
    if (!map) return;
    
    auto* tile = map->getTile(new_pos);
    if (!tile) {
        return; // No tile exists
    }
    
    if (!tile->hasGround()) {
        return; // No ground
    }
    
    // Check ground item for walkable/blocking flags
    if (client_data) {
        auto* ground = tile->getGround();
        if (ground) {
            auto* ground_type = client_data->getItemTypeByServerId(ground->getServerId());
            if (ground_type) {
                // Check if ground itself blocks
                if (ground_type->is_blocking || 
                    ground_type->hasFlag(Domain::ItemFlag::Unpassable) ||
                    ground_type->hasFlag(Domain::ItemFlag::BlockPathfinder)) {
                    return; // Ground is not walkable
                }
            }
        }
        
        // Check all items on tile for blocking
        for (const auto& item : tile->getItems()) {
            if (item) {
                auto* item_type = client_data->getItemTypeByServerId(item->getServerId());
                if (item_type && (item_type->is_blocking || 
                    item_type->hasFlag(Domain::ItemFlag::Unpassable) ||
                    item_type->hasFlag(Domain::ItemFlag::BlockPathfinder))) {
                    return; // Blocked by item (wall, etc)
                }
            }
        }
    }
    
    // Check if tile already has a creature
    if (tile->hasCreature()) {
        return; // Occupied by real creature
    }
    
    // Check if another simulated creature is at target
    // Optimization: Use O(1) lookup instead of O(N) loop
    if (occupied_positions_.count(new_pos)) {
        return; // Occupied by simulated creature
    }
    
    // Move is valid - update spatial index
    auto it = occupied_positions_.find(state.current_pos);
    if (it != occupied_positions_.end()) {
        if (--(it->second) == 0) {
            occupied_positions_.erase(it);
        }
    }
    occupied_positions_[new_pos]++;

    // Start walk animation
    state.current_pos = new_pos;
    state.direction = static_cast<uint8_t>(new_dir);
    state.is_walking = true;
    state.walk_progress = 0.0f;
    state.animation_frame = 1;
    
    // Initial offset (creature starts from previous tile)
    state.walk_offset_x = -dx;
    state.walk_offset_y = -dy;
}

CreatureAnimState* CreatureSimulator::getOrCreateState(
    const Domain::Creature* creature,
    const Domain::Position& position,
    Domain::ChunkedMap* map) {
    
    if (!enabled_ || !creature) return nullptr;
    
    // Create key based on creature pointer (address)
    // This is O(1) and stable as long as the Tile owns the unique_ptr<Creature>
    uint64_t const key = makeKey(creature);
    
    auto it = states_.find(key);
    if (it != states_.end()) {
        return &it->second;
    }
    
    // Find the spawn that contains this creature's position
    // Search for a spawn tile within reasonable distance
    int spawn_radius = DEFAULT_ROAM_RADIUS; // Default if no spawn found
    Domain::Position spawn_center = position; // Default to creature position
    
    if (map) {
        // Search for spawn tile in area around creature using chunk optimization
        // Instead of checking 441 tiles (21x21), we check chunks in the region
        // and iterate their spawn cache.
        std::vector<Domain::Chunk*> chunks;
        map->getVisibleChunks(
            position.x - 10, position.y - 10,
            position.x + 10, position.y + 10,
            position.z, chunks
        );

        for (auto* chunk : chunks) {
            if (!chunk->hasSpawns()) continue;

            for (const auto* check_tile : chunk->getSpawnTiles()) {
                auto spawn = check_tile->getSpawn();
                if (spawn) {
                    // Check if creature position is within this spawn's radius
                    int const dist_x = std::abs(position.x - check_tile->getX());
                    int const dist_y = std::abs(position.y - check_tile->getY());
                    if (dist_x <= spawn->radius && dist_y <= spawn->radius) {
                        spawn_center = check_tile->getPosition();
                        spawn_radius = spawn->radius;
                        goto found_spawn;
                    }
                }
            }
        }
    }
    found_spawn:
    
    // Create new state
    CreatureAnimState state;
    state.creature_name = creature->name;
    state.spawn_center = spawn_center;
    state.spawn_radius = spawn_radius;
    state.original_offset_x = position.x - spawn_center.x;
    state.original_offset_y = position.y - spawn_center.y;
    state.current_pos = position;  // Start at creature's actual position
    state.direction = static_cast<uint8_t>(creature->direction);
    state.animation_frame = 0;
    state.move_timer = TICK_INTERVAL_SEC;
    state.is_walking = false;
    state.walk_progress = 0.0f;
    state.walk_offset_x = 0.0f;
    state.walk_offset_y = 0.0f;
    
    auto [inserted_it, success] = states_.emplace(key, std::move(state));

    // Add to spatial index if on current floor (though update() will rebuild it anyway)
    // We assume current floor management is handled in update(), but safe to add if needed.
    // For now, let update() handle the index population to keep it consistent with the viewport.

    return &inserted_it->second;
}

const CreatureAnimState* CreatureSimulator::getState(
    const Domain::Position& spawn_center,
    const std::string& creature_name) {
    
    // This legacy lookup by name/pos is broken with pointer-based keys.
    // However, it's not used in hot paths (rendering uses getOrCreateState with pointer).
    // If needed, we would need a reverse mapping or revert to expensive keys.
    // For now, return nullptr as this signature implies value-based lookup which we are replacing.
    return nullptr;
}

void CreatureSimulator::reset() {
    states_.clear();
    occupied_positions_.clear();
}

uint64_t CreatureSimulator::makeKey(const Domain::Creature* creature) {
    // Use the pointer address as the key.
    // Domain::Creature objects are stable in memory (owned by Tile via unique_ptr).
    // This eliminates string hashing and position hashing every frame.
    return reinterpret_cast<uint64_t>(creature);
}

} // namespace Services
} // namespace MapEditor
