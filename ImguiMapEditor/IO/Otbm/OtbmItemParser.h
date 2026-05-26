#pragma once
#include "../NodeFileReader.h"
#include "Domain/Item.h"
#include <memory>
#include <cstdint>

namespace MapEditor {
namespace Services {
    class ClientDataService;
}
namespace IO {

enum class OtbmVersion : uint32_t;
enum class OtbmAttribute : uint8_t;

/**
 * Parses OTBM item nodes and their attributes.
 * 
 * Single responsibility: item deserialization from OTBM format.
 */
class OtbmItemParser {
public:
    /**
     * Parse an item from a binary node.
     * @param node The binary node containing item data
     * @param version OTBM version for format differences
     * @param client_data Optional client data for ItemType lookup
     * @return Parsed item or nullptr on failure
     */
    static std::unique_ptr<Domain::Item> parseItem(BinaryNode* node, 
                                                    OtbmVersion version,
                                                    Services::ClientDataService* client_data);

    /**
     * Parse item attributes (action ID, unique ID, text, etc.)
     * @param node The binary node positioned after the item ID
     * @param item Item to populate
     * @return true if successful
     */
    static bool parseItemAttributes(BinaryNode* node, Domain::Item& item);

    /**
     * Parse container child items recursively.
     * @param node Parent item node
     * @param item Container item to add children to
     * @param version OTBM version
     * @param client_data Client data service
     * @return true if successful
     */
    static bool parseItemChildren(BinaryNode* node, Domain::Item& item,
                                    OtbmVersion version,
                                    Services::ClientDataService* client_data,
                                    int depth = 0);

    /**
     * Parse OTBM v4 attribute map.
     * @param node Binary node
     * @param item Item to populate
     * @return true if successful
     */
    static bool parseAttributeMap(BinaryNode* node, Domain::Item& item);

private:
    OtbmItemParser() = delete;
};

} // namespace IO
} // namespace MapEditor
