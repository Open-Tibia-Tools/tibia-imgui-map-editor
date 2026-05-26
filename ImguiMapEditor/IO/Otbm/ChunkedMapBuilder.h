#pragma once
#include "IMapBuilder.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include "Domain/Spawn.h"
#include "Domain/Creature.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace IO {

/**
 * IMapBuilder implementation for Domain::ChunkedMap.
 * Provides a polymorphic way to populate ChunkedMap during OTBM parsing.
 */
class ChunkedMapBuilder : public IMapBuilder {
public:
    explicit ChunkedMapBuilder(Domain::ChunkedMap& map) : map_(map) {}
    ~ChunkedMapBuilder() override = default;

    void setSize(uint16_t width, uint16_t height) override {
        map_.setSize(width, height);
    }

    void setSpawnFile(const std::string& filename) override {
        spawn_file_ = filename;
        map_.setSpawnFile(filename);
        spdlog::info("ChunkedMapBuilder: Set spawn file on map: {}", filename);
    }

    void setHouseFile(const std::string& filename) override {
        house_file_ = filename;
        map_.setHouseFile(filename);
        spdlog::info("ChunkedMapBuilder: Set house file on map: {}", filename);
    }

    void setDescription(const std::string& description) override {
        map_.setDescription(description);
    }

    Domain::Tile* getOrCreateTile(const Domain::Position& pos) override {
        return map_.getOrCreateTile(pos);
    }

    void setTile(const Domain::Position& pos, std::unique_ptr<Domain::Tile> tile) override {
        map_.setTile(pos, std::move(tile));
    }

    bool hasTown(uint32_t id) const override {
        return map_.getTown(id) != nullptr;
    }

    void addTown(uint32_t id, const std::string& name, const Domain::Position& temple_pos) override {
        map_.addTown(id, name, temple_pos);
    }

    void addWaypoint(const std::string& name, const Domain::Position& pos) override {
        map_.addWaypoint(name, pos);
    }

    void setSpawn(const Domain::Position& pos, std::unique_ptr<Domain::Spawn> spawn) override {
        Domain::Tile* tile = map_.getOrCreateTile(pos);
        if (tile) {
            tile->setSpawn(std::move(spawn));
        }
    }
    
    void setCreature(const Domain::Position& pos, std::unique_ptr<Domain::Creature> creature) override {
        Domain::Tile* tile = map_.getOrCreateTile(pos);
        if (tile) {
            tile->setCreature(std::move(creature));
        }
    }

    void ensureHouse(uint32_t house_id) override {
        auto* existing = map_.getHouse(house_id);
        if (!existing) {
            map_.addHouse(std::make_unique<Domain::House>(house_id));
        }
    }

    // Accessors for external files
    const std::string& getSpawnFile() const { return spawn_file_; }
    const std::string& getHouseFile() const { return house_file_; }

private:
    Domain::ChunkedMap& map_;
    std::string spawn_file_;
    std::string house_file_;
};

} // namespace IO
} // namespace MapEditor

