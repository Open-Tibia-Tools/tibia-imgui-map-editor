#pragma once
#include "IMapBuilder.h"
#include "../NodeFileReader.h"
#include <cstdint>
#include <string>

namespace MapEditor {
namespace Services {
    class ClientDataService;
}
namespace IO {

enum class OtbmVersion : uint32_t;
struct OtbmResult;

/**
 * Parses tile-related OTBM nodes: tiles, spawns, towns, waypoints.
 * 
 * Single responsibility: tile and entity deserialization from OTBM format.
 */
class OtbmTileParser {
public:
    /**
     * Parse a tile area node and all its tiles.
     */
    static bool parseTileArea(BinaryNode* tileAreaNode, 
                               IMapBuilder& builder,
                               OtbmResult& result, 
                               Services::ClientDataService* client_data);
    
    /**
     * Parse a single tile node.
     */
    static bool parseTile(BinaryNode* tileNode, 
                          IMapBuilder& builder,
                          uint16_t base_x, uint16_t base_y, uint8_t base_z,
                          OtbmResult& result, 
                          Services::ClientDataService* client_data);
    
    /**
     * Parse towns node.
     */
    static bool parseTowns(BinaryNode* townsNode, 
                           IMapBuilder& builder, 
                           OtbmResult& result);
    
    /**
     * Parse waypoints node.
     */
    static bool parseWaypoints(BinaryNode* waypointsNode, 
                               IMapBuilder& builder, 
                               OtbmResult& result);

private:
    OtbmTileParser() = delete;
};

} // namespace IO
} // namespace MapEditor
