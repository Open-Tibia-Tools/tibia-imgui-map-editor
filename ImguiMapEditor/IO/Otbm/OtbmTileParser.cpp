#include "OtbmTileParser.h"
#include "OtbmItemParser.h"
#include "OtbmReader.h"
#include "Domain/Tile.h"
#include "Domain/Spawn.h"
#include "Domain/Creature.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace IO {

bool OtbmTileParser::parseTileArea(BinaryNode* tileAreaNode, 
                                    IMapBuilder& builder,
                                    OtbmResult& result, 
                                    Services::ClientDataService* client_data) {
    uint16_t base_x, base_y;
    uint8_t base_z;
    
    if (!tileAreaNode->getU16(base_x) || 
        !tileAreaNode->getU16(base_y) || 
        !tileAreaNode->getU8(base_z)) {
        return false;
    }
    
    for (auto& tileNode : tileAreaNode->children()) {
        if (!parseTile(&tileNode, builder, base_x, base_y, base_z, result, client_data)) {
            spdlog::trace("Failed to parse tile in area ({},{},{})", 
                         base_x, base_y, base_z);
        }
    }
    
    return true;
}

bool OtbmTileParser::parseTile(BinaryNode* tileNode, 
                                IMapBuilder& builder,
                                uint16_t base_x, uint16_t base_y, uint8_t base_z,
                                OtbmResult& result, 
                                Services::ClientDataService* client_data) {
    uint8_t tile_type;
    if (!tileNode->getU8(tile_type)) {
        return false;
    }
    
    if (tile_type != static_cast<uint8_t>(OtbmNode::Tile) &&
        tile_type != static_cast<uint8_t>(OtbmNode::HouseTile)) {
        return false;
    }
    
    uint8_t x_offset, y_offset;
    if (!tileNode->getU8(x_offset) || !tileNode->getU8(y_offset)) {
        return false;
    }
    
    Domain::Position const pos(
        static_cast<int32_t>(base_x + x_offset),
        static_cast<int32_t>(base_y + y_offset),
        static_cast<int16_t>(base_z)
    );
    
    auto tile = std::make_unique<Domain::Tile>(pos);
    
    uint32_t house_id = 0;
    if (tile_type == static_cast<uint8_t>(OtbmNode::HouseTile)) {
        if (!tileNode->getU32(house_id)) {
            return false;
        }
        tile->setHouseId(house_id);
    }
    
    OtbmVersion const otbm_ver = static_cast<OtbmVersion>(result.version.otbm_version);
    
    // Track if we've set ground yet - first item in OTBM is always ground
    bool ground_set = false;
    
    // Read tile attributes - using a flag to break out of loop
    bool done_attributes = false;
    uint8_t attr;
    while (!done_attributes && tileNode->getU8(attr)) {
        switch (attr) {
            case static_cast<uint8_t>(OtbmAttribute::TileFlags): {
                uint32_t flags;
                if (tileNode->getU32(flags)) {
                    tile->setFlags(flags);
                }
                break;
            }
            case static_cast<uint8_t>(OtbmAttribute::Item): {
                auto item = OtbmItemParser::parseItem(tileNode, otbm_ver, client_data);
                if (item) {
                    // Parse item attributes (count, charges, etc.)
                    OtbmItemParser::parseItemAttributes(tileNode, *item);
                    
                    result.item_count++;
                    
                    // OTBM format: first item in tile attributes is ALWAYS ground
                    // Even if ItemType is null (invalid), we must set it as ground
                    if (!ground_set) {
                        tile->setGround(std::move(item));
                        ground_set = true;
                    } else {
                        tile->addItem(std::move(item));
                    }
                }
                break;
            }
            default:
                done_attributes = true;
                break;
        }
    }

    // Read item child nodes
    // NOTE: ground_set carries over from attributes - if no ground was set there,
    // the first valid child node should be treated as ground
    for (auto& itemNode : tileNode->children()) {
        uint8_t item_type;
        if (!itemNode.getU8(item_type)) {
            continue;
        }
        
        if (item_type != static_cast<uint8_t>(OtbmNode::Item)) {
            continue;
        }
        
        auto item = OtbmItemParser::parseItem(&itemNode, otbm_ver, client_data);
        if (item) {
            OtbmItemParser::parseItemAttributes(&itemNode, *item);
            OtbmItemParser::parseItemChildren(&itemNode, *item, otbm_ver, client_data);
            
            result.item_count++;
            
            // If no ground has been set yet (not from attributes, tile is empty),
            // treat the first item as ground - even if it has no valid ItemType
            if (!ground_set && !tile->getGround()) {
                tile->setGround(std::move(item));
                ground_set = true;
            } else {
                // Use addItemDirect to bypass ground-promotion logic
                // Child items from OTBM should NOT replace ground, even if they're ground-type
                tile->addItemDirect(std::move(item));
            }
        }
    }
    
    builder.setTile(pos, std::move(tile));
    result.tile_count++;
    
    return true;
}

bool OtbmTileParser::parseSpawns(BinaryNode* spawnsNode, 
                                  IMapBuilder& builder, 
                                  OtbmResult& result) {
    for (auto& spawnAreaNode : spawnsNode->children()) {
        uint8_t type;
        if (!spawnAreaNode.getU8(type)) continue;
        if (type != static_cast<uint8_t>(OtbmNode::SpawnArea)) continue;
        
        uint16_t x, y, radius;
        uint8_t z;
        if (!spawnAreaNode.getU16(x) || !spawnAreaNode.getU16(y) ||
            !spawnAreaNode.getU8(z) || !spawnAreaNode.getU16(radius)) {
            continue;
        }

        Domain::Position const pos(x, y, z);
        auto spawn = std::make_unique<Domain::Spawn>();
        spawn->position = pos;
        spawn->radius = radius;
        
        // Parse creatures and tell builder to create them on tiles
        for (auto& monsterNode : spawnAreaNode.children()) {
             uint8_t mType;
             if (!monsterNode.getU8(mType)) continue;
             if (mType != static_cast<uint8_t>(OtbmNode::Monster)) continue;
             
             uint16_t mx, my, spawn_time;
             std::string name;
             
             if (!monsterNode.getU16(mx) || !monsterNode.getU16(my) ||
                 !monsterNode.getString(name) || !monsterNode.getU16(spawn_time)) {
                 continue;
             }
             
             // Create creature at absolute position (spawn + offset)
             Domain::Position const creature_pos(pos.x + mx, pos.y + my, pos.z);
             auto creature = std::make_unique<Domain::Creature>(name, spawn_time, 2); // Default: South
             builder.setCreature(creature_pos, std::move(creature));
        }
        
        builder.setSpawn(pos, std::move(spawn));
    }
    return true;
}

bool OtbmTileParser::parseTowns(BinaryNode* townsNode, 
                                 IMapBuilder& builder, 
                                 OtbmResult& result) {
    for (auto& townNode : townsNode->children()) {
        uint8_t type;
        if (!townNode.getU8(type)) continue;
        
        if (type != static_cast<uint8_t>(OtbmNode::Town)) continue;
        
        uint32_t town_id;
        if (!townNode.getU32(town_id)) continue;
        
        std::string name;
        if (!townNode.getString(name)) continue;
        
        uint16_t x, y;
        uint8_t z;
        if (!townNode.getU16(x) || !townNode.getU16(y) || !townNode.getU8(z)) {
            continue;
        }
        
        Domain::Position const temple_pos(x, y, z);
        builder.addTown(town_id, name, temple_pos);
        result.town_count++;
    }
    return true;
}

bool OtbmTileParser::parseWaypoints(BinaryNode* waypointsNode, 
                                     IMapBuilder& builder,
                                     OtbmResult& result) {
    for (auto& wpNode : waypointsNode->children()) {
        uint8_t type;
        if (!wpNode.getU8(type)) continue;
        
        if (type != static_cast<uint8_t>(OtbmNode::Waypoint)) continue;
        
        std::string name;
        if (!wpNode.getString(name)) continue;
        
        uint16_t x, y;
        uint8_t z;
        if (!wpNode.getU16(x) || !wpNode.getU16(y) || !wpNode.getU8(z)) {
            continue;
        }
        
        Domain::Position const pos(x, y, z);
        builder.addWaypoint(name, pos);
        result.waypoint_count++;
    }
    return true;
}

} // namespace IO
} // namespace MapEditor
