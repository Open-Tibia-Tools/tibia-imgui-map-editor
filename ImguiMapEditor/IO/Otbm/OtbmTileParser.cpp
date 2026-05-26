#include "OtbmTileParser.h"
#include "OtbmItemParser.h"
#include "OtbmReader.h"
#include "OtbmOpaqueData.h"
#include "Domain/Tile.h"
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
    
    Domain::Position pos(
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
        builder.ensureHouse(house_id);
    }
    
    OtbmVersion otbm_ver = static_cast<OtbmVersion>(result.version.otbm_version);
    
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
                    
                    // Capture unknown flag bits for preservation
                    constexpr uint32_t KNOWN_FLAGS_MASK = 0x3F;
                    uint32_t unknown_flags = flags & ~KNOWN_FLAGS_MASK;
                    if (unknown_flags != 0) {
                        auto opaque = std::make_unique<InvalidZoneState>();
                        opaque->unknownMapFlags = unknown_flags;
                        tile->setOpaqueData(std::move(opaque));
                    }
                }
                break;
            }
            case static_cast<uint8_t>(OtbmAttribute::Item): {
                auto item = OtbmItemParser::parseItem(tileNode, otbm_ver, client_data);
                if (item) {
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
            default: {
                // Unknown tile attribute - capture raw bytes for preservation
                size_t remaining = tileNode->bytesRemaining();
                auto* opaque = tile->getOpaqueData();
                if (!opaque) {
                    auto newOpaque = std::make_unique<InvalidZoneState>();
                    opaque = newOpaque.get();
                    tile->setOpaqueData(std::move(newOpaque));
                }
                OpaqueTileAttribute oa;
                oa.attributeId = attr;
                oa.rawBytes.resize(remaining);
                if (remaining > 0) {
                    tileNode->getRAW(oa.rawBytes.data(), remaining);
                }
                opaque->opaqueAttributes.push_back(std::move(oa));
                done_attributes = true;
                break;
            }
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
            // Unknown child node type - capture raw bytes for preservation
            size_t remaining = itemNode.bytesRemaining();
            auto* opaque = tile->getOpaqueData();
            if (!opaque) {
                auto newOpaque = std::make_unique<InvalidZoneState>();
                opaque = newOpaque.get();
                tile->setOpaqueData(std::move(newOpaque));
            }
            OpaqueChildNode ocn;
            ocn.nodeType = item_type;
            ocn.rawBytes.resize(remaining);
            if (remaining > 0) {
                itemNode.getRAW(ocn.rawBytes.data(), remaining);
            }
            opaque->opaqueChildNodes.push_back(std::move(ocn));
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

bool OtbmTileParser::parseTowns(BinaryNode* townsNode, 
                                 IMapBuilder& builder, 
                                 OtbmResult& result) {
    for (auto& townNode : townsNode->children()) {
        uint8_t type;
        if (!townNode.getU8(type)) continue;
        
        if (type != static_cast<uint8_t>(OtbmNode::Town)) continue;
        
        uint32_t town_id;
        if (!townNode.getU32(town_id)) continue;
        
        if (builder.hasTown(town_id)) {
            spdlog::warn("Duplicate town ID {}, skipping", town_id);
            continue;
        }
        
        std::string name;
        if (!townNode.getString(name)) continue;
        
        uint16_t x, y;
        uint8_t z;
        if (!townNode.getU16(x) || !townNode.getU16(y) || !townNode.getU8(z)) {
            continue;
        }
        
        if (x == 0 && y == 0) {
            spdlog::warn("Town '{}' ({}) has temple position (0,0), skipping", name, town_id);
            continue;
        }
        
        Domain::Position temple_pos(x, y, z);
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
        
        Domain::Position pos(x, y, z);
        builder.addWaypoint(name, pos);
        result.waypoint_count++;
    }
    return true;
}

} // namespace IO
} // namespace MapEditor
