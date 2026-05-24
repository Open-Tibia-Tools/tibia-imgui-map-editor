#include "OtbmIdConverter.h"
#include "OtbmReader.h"
#include "../NodeFileReader.h"
#include "../NodeFileWriter.h"
#include "Services/ClientDataService.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace MapEditor::IO {

namespace {

// Helper to convert a single ID
uint16_t convertId(uint16_t id, ConversionDirection direction, 
                   Services::ClientDataService* client_data,
                   size_t& converted, size_t& skipped) {
    if (id == 0 || !client_data) {
        return id;
    }
    
    if (direction == ConversionDirection::ServerToClient) {
        const auto* type = client_data->getItemTypeByServerId(id);
        if (type && type->client_id > 0) {
            converted++;
            return type->client_id;
        }
    } else {
        const auto* type = client_data->getItemTypeByClientId(id);
        if (type && type->server_id > 0) {
            converted++;
            return type->server_id;
        }
    }
    
    skipped++;
    return id;
}

// Recursive node processor that copies nodes while converting item IDs
void processNode(BinaryNode* node, NodeFileWriteHandle& writer, 
                 ConversionDirection direction, Services::ClientDataService* client_data,
                 size_t& converted, size_t& skipped, uint8_t parent_type) {
    
    // Read the node type (first byte after node start)
    uint8_t node_type;
    if (!node->getU8(node_type)) {
        return;
    }
    
    writer.startNode(node_type);
    
    // Handle different node types
    if (node_type == static_cast<uint8_t>(OtbmNode::Item)) {
        // Item node: first 2 bytes after type are the item ID
        uint16_t item_id;
        if (node->getU16(item_id)) {
            uint16_t const new_id = convertId(item_id, direction, client_data, converted, skipped);
            writer.writeU16(new_id);
        }
        
        // Copy remaining bytes (attributes) as-is
        size_t const remaining = node->bytesRemaining();
        if (remaining > 0) {
            std::string data;
            if (node->getRAW(data, remaining)) {
                writer.writeRAW(data);
            }
        }
    } 
    else if (node_type == static_cast<uint8_t>(OtbmNode::Tile) ||
             node_type == static_cast<uint8_t>(OtbmNode::HouseTile)) {
        // Tile nodes have: x_offset, y_offset, [house_id for HouseTile], then attributes
        // We need to parse attributes to find inline items (OtbmAttribute::Item)
        
        uint8_t x_offset, y_offset;
        if (!node->getU8(x_offset) || !node->getU8(y_offset)) {
            writer.endNode();
            return;
        }
        writer.writeU8(x_offset);
        writer.writeU8(y_offset);
        
        if (node_type == static_cast<uint8_t>(OtbmNode::HouseTile)) {
            uint32_t house_id;
            if (node->getU32(house_id)) {
                writer.writeU32(house_id);
            }
        }
        
        // Parse tile attributes - looking for inline items
        // Must handle ALL known attribute types to avoid missing Item conversions
        uint8_t attr;
        while (node->bytesRemaining() > 0) {
            if (!node->getU8(attr)) break;
            
            switch (attr) {
                case static_cast<uint8_t>(OtbmAttribute::TileFlags): {
                    writer.writeU8(attr);
                    uint32_t flags;
                    if (node->getU32(flags)) {
                        writer.writeU32(flags);
                    }
                    break;
                }
                case static_cast<uint8_t>(OtbmAttribute::Item): {
                    // Inline item - just ID, needs conversion
                    uint16_t item_id;
                    if (node->getU16(item_id)) {
                        uint16_t const new_id = convertId(item_id, direction, client_data, converted, skipped);
                        writer.writeU8(attr);
                        writer.writeU16(new_id);
                    }
                    break;
                }
                // U8 attributes (1 byte)
                case static_cast<uint8_t>(OtbmAttribute::Count):
                case static_cast<uint8_t>(OtbmAttribute::RuneCharges):
                case static_cast<uint8_t>(OtbmAttribute::HouseDoorId):
                case static_cast<uint8_t>(OtbmAttribute::Tier): {
                    writer.writeU8(attr);
                    uint8_t val;
                    if (node->getU8(val)) {
                        writer.writeU8(val);
                    }
                    break;
                }
                // U16 attributes (2 bytes)
                case static_cast<uint8_t>(OtbmAttribute::Charges):
                case static_cast<uint8_t>(OtbmAttribute::ActionId):
                case static_cast<uint8_t>(OtbmAttribute::UniqueId):
                case static_cast<uint8_t>(OtbmAttribute::DepotId): {
                    writer.writeU8(attr);
                    uint16_t val;
                    if (node->getU16(val)) {
                        writer.writeU16(val);
                    }
                    break;
                }
                // U32 attributes (4 bytes)
                case static_cast<uint8_t>(OtbmAttribute::Duration):
                case static_cast<uint8_t>(OtbmAttribute::DecayingState):
                case static_cast<uint8_t>(OtbmAttribute::WrittenDate):
                case static_cast<uint8_t>(OtbmAttribute::SleeperGuid):
                case static_cast<uint8_t>(OtbmAttribute::SleepStart): {
                    writer.writeU8(attr);
                    uint32_t val;
                    if (node->getU32(val)) {
                        writer.writeU32(val);
                    }
                    break;
                }
                // String attributes
                case static_cast<uint8_t>(OtbmAttribute::Text):
                case static_cast<uint8_t>(OtbmAttribute::Desc):
                case static_cast<uint8_t>(OtbmAttribute::Description):
                case static_cast<uint8_t>(OtbmAttribute::ExtFile):
                case static_cast<uint8_t>(OtbmAttribute::ExtSpawnFile):
                case static_cast<uint8_t>(OtbmAttribute::ExtHouseFile):
                case static_cast<uint8_t>(OtbmAttribute::WrittenBy): {
                    writer.writeU8(attr);
                    std::string str;
                    if (node->getString(str)) {
                        writer.writeString(str);
                    }
                    break;
                }
                // TeleportDest: x(U16) + y(U16) + z(U8) = 5 bytes
                case static_cast<uint8_t>(OtbmAttribute::TeleportDest): {
                    writer.writeU8(attr);
                    uint16_t x, y;
                    uint8_t z;
                    if (node->getU16(x) && node->getU16(y) && node->getU8(z)) {
                        writer.writeU16(x);
                        writer.writeU16(y);
                        writer.writeU8(z);
                    }
                    break;
                }
                // PodiumOutfit: 15 bytes fixed
                case static_cast<uint8_t>(OtbmAttribute::PodiumOutfit): {
                    writer.writeU8(attr);
                    std::string data;
                    if (node->getRAW(data, 15)) {
                        writer.writeRAW(data);
                    }
                    break;
                }
                // AttributeMap: complex structure, copy remaining bytes
                case static_cast<uint8_t>(OtbmAttribute::AttributeMap): {
                    writer.writeU8(attr);
                    // AttributeMap is variable-length and complex, copy all remaining data
                    size_t const remaining = node->bytesRemaining();
                    if (remaining > 0) {
                        std::string data;
                        if (node->getRAW(data, remaining)) {
                            writer.writeRAW(data);
                        }
                    }
                    // AttributeMap is always last, break out of loop
                    goto done_attributes;
                }
                default: {
                    // Unknown attribute - no child nodes expected in tile attributes
                    // End of attributes section - likely reading into child node area
                    // Write the byte back and stop parsing
                    spdlog::trace("OtbmIdConverter: Unknown tile attribute {} at position, stopping", attr);
                    writer.writeU8(attr);
                    size_t const remaining = node->bytesRemaining();
                    if (remaining > 0) {
                        std::string data;
                        if (node->getRAW(data, remaining)) {
                            writer.writeRAW(data);
                        }
                    }
                    goto done_attributes;
                }
            }
        }
        done_attributes:;
    }
    else {
        // All other nodes: copy data as-is
        size_t const remaining = node->bytesRemaining();
        if (remaining > 0) {
            std::string data;
            if (node->getRAW(data, remaining)) {
                writer.writeRAW(data);
            }
        }
    }
    
    // Process children recursively
    for (auto& child : node->children()) {
        processNode(&child, writer, direction, client_data, converted, skipped, node_type);
    }
    
    writer.endNode();
}

} // anonymous namespace

OtbmConvertResult OtbmIdConverter::convert(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    ConversionDirection direction,
    Services::ClientDataService* client_data
) {
    OtbmConvertResult result;
    
    if (!client_data) {
        result.error = "Client data required for conversion";
        return result;
    }
    
    // Open input file
    DiskNodeFileReadHandle reader(input_path, {"OTBM", std::string(4, '\0')});
    if (!reader.isOk()) {
        result.error = "Failed to open input file: " + reader.getErrorMessage();
        return result;
    }
    
    // Open output file
    NodeFileWriteHandle writer(output_path, "OTBM");
    if (!writer.isOk()) {
        result.error = "Failed to open output file for writing";
        return result;
    }
    
    // Get root node
    BinaryNode* root = reader.getRootNode();
    if (!root) {
        result.error = "Failed to read root node";
        return result;
    }
    
    // Process all nodes recursively
    processNode(root, writer, direction, client_data, 
                result.items_converted, result.items_skipped, 0);
    
    writer.close();
    
    if (!writer.isOk()) {
        result.error = "Error writing output file";
        return result;
    }
    
    result.success = true;
    
    spdlog::info("OTBM ID conversion complete: {} items converted, {} skipped",
                result.items_converted, result.items_skipped);
    
    return result;
}

} // namespace MapEditor::IO
