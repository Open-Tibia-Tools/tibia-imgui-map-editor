#pragma once
#include "Domain/Position.h"
#include <memory>
#include <string>
#include <cstdint>

namespace MapEditor {

namespace Domain {
    class Tile;
    class Town;
    class Spawn;
    struct Waypoint;
    struct Creature;
}

namespace IO {

/**
 * Interface for building map structures during parsing.
 * Replaces template duplication in OtbmReader by allowing
 * different map types to implement this interface.
 */
class IMapBuilder {
public:
    virtual ~IMapBuilder() = default;

    // Map dimensions
    virtual void setSize(uint16_t width, uint16_t height) = 0;
    virtual void setSpawnFile(const std::string& filename) = 0;
    virtual void setHouseFile(const std::string& filename) = 0;
    virtual void setDescription(const std::string& description) = 0;

    // Tile operations
    virtual Domain::Tile* getOrCreateTile(const Domain::Position& pos) = 0;
    virtual void setTile(const Domain::Position& pos, std::unique_ptr<Domain::Tile> tile) = 0;

    // Entity operations
    virtual bool hasTown(uint32_t id) const = 0;
    virtual void addTown(uint32_t id, const std::string& name, const Domain::Position& temple_pos) = 0;
    virtual void addWaypoint(const std::string& name, const Domain::Position& pos) = 0;
    virtual void setSpawn(const Domain::Position& pos, std::unique_ptr<Domain::Spawn> spawn) = 0;
    
    // Creature operation (per-tile storage like RME)
    virtual void setCreature(const Domain::Position& pos, std::unique_ptr<Domain::Creature> creature) = 0;

    // House auto-creation from HouseTile nodes
    virtual void ensureHouse(uint32_t house_id) = 0;
    virtual void addTileToHouse(uint32_t house_id, const Domain::Position& pos) = 0;
};

} // namespace IO
} // namespace MapEditor

