#include "OtbmItemParser.h"
#include "OtbmReader.h"
#include "Services/ClientDataService.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace MapEditor {
namespace IO {

std::unique_ptr<Domain::Item> OtbmItemParser::parseItem(BinaryNode* node, 
                                                         OtbmVersion version,
                                                         Services::ClientDataService* client_data) {
    uint16_t server_id;
    if (!node->getU16(server_id)) {
        return nullptr;
    }
    
    auto item = std::make_unique<Domain::Item>();
    item->setServerId(server_id);
    
    // Assign ItemType if client data is available
    if (client_data) {
        const Domain::ItemType* type = client_data->getItemTypeByServerId(server_id);
        if (type) {
            item->setType(type);
        }
    }
    
    // MAP_OTBM_1: inline count/subtype for stackable/splash/fluid items
    if (version == OtbmVersion::V1) {
        const Domain::ItemType* type = item->getType();
        if (type) {
            if (type->is_stackable || type->isSplash() || type->isFluidContainer()) {
                uint8_t count;
                if (!node->getU8(count)) return nullptr;
                item->setSubtype(count);
            }
        } else {
            // Type unknown — V1 requires type metadata to know if inline count follows.
            // Always consume 1 byte as count to prevent stream corruption.
            uint8_t count;
            if (!node->getU8(count)) return nullptr;
            item->setSubtype(count);
        }
    }
    
    return item;
}

bool OtbmItemParser::parseItemAttributes(BinaryNode* node, Domain::Item& item) {
    uint8_t attr;
    while (node->peekU8(attr)) {
        // First check if it's a known attribute for Items
        // If not, we must NOT consume it, as it belongs to the parent (Tile) parser
        switch (static_cast<OtbmAttribute>(attr)) {
            case OtbmAttribute::Count:
            case OtbmAttribute::RuneCharges:
            case OtbmAttribute::Charges:
            case OtbmAttribute::ActionId:
            case OtbmAttribute::UniqueId:
            case OtbmAttribute::Text:
            case OtbmAttribute::Desc:
            case OtbmAttribute::TeleportDest:
            case OtbmAttribute::DepotId:
            case OtbmAttribute::HouseDoorId:
            case OtbmAttribute::Tier:
            case OtbmAttribute::PodiumOutfit:
            case OtbmAttribute::AttributeMap:
                // Known attribute - consume and process
                node->getU8(attr);
                break;
            default:
                // Unknown attribute (likely start of parent's next attribute) - stop reading
                return true;
        }

        switch (static_cast<OtbmAttribute>(attr)) {
            case OtbmAttribute::Count:
            case OtbmAttribute::RuneCharges: {
                uint8_t count;
                if (node->getU8(count)) {
                    item.setSubtype(count);
                }
                break;
            }
            case OtbmAttribute::Charges: {
                uint16_t charges;
                if (node->getU16(charges)) {
                    item.setCharges(static_cast<uint8_t>(charges));
                }
                break;
            }
            case OtbmAttribute::ActionId: {
                uint16_t aid;
                if (node->getU16(aid)) {
                    item.setActionId(aid);
                }
                break;
            }
            case OtbmAttribute::UniqueId: {
                uint16_t uid;
                if (node->getU16(uid)) {
                    item.setUniqueId(uid);
                }
                break;
            }
            case OtbmAttribute::Text: {
                std::string text;
                if (node->getString(text)) {
                    item.setText(text);
                }
                break;
            }
            case OtbmAttribute::Desc: {
                std::string desc;
                if (node->getString(desc)) {
                    item.setDescription(desc);
                }
                break;
            }
            case OtbmAttribute::TeleportDest: {
                uint16_t x, y;
                uint8_t z;
                if (node->getU16(x) && node->getU16(y) && node->getU8(z)) {
                    Domain::Position dest(x, y, z);
                    item.setTeleportDestination(dest);
                }
                break;
            }
            case OtbmAttribute::DepotId: {
                uint16_t depot_id;
                if (node->getU16(depot_id)) {
                    if (depot_id > 255) {
                        spdlog::error("Depot ID too large: {}", depot_id);
                        return false;
                    }
                    item.setDepotId(static_cast<uint32_t>(depot_id));
                }
                break;
            }
            case OtbmAttribute::HouseDoorId: {
                uint8_t door_id;
                if (node->getU8(door_id)) {
                    item.setDoorId(static_cast<uint32_t>(door_id));
                }
                break;
            }
            case OtbmAttribute::Tier: {
                uint8_t tier;
                if (node->getU8(tier)) {
                    item.setTier(tier);
                }
                break;
            }
            case OtbmAttribute::PodiumOutfit: {
                uint8_t flags, direction, lookHead, lookBody, lookLegs, lookFeet, lookAddon;
                uint8_t lookMountHead, lookMountBody, lookMountLegs, lookMountFeet;
                uint16_t lookType, lookMount;
                
                if (!node->getU8(flags) || !node->getU8(direction)) break;
                if (!node->getU16(lookType)) break;
                if (!node->getU8(lookHead) || !node->getU8(lookBody) || !node->getU8(lookLegs) || !node->getU8(lookFeet) || !node->getU8(lookAddon)) break;
                if (!node->getU16(lookMount)) break;
                if (!node->getU8(lookMountHead) || !node->getU8(lookMountBody) || !node->getU8(lookMountLegs) || !node->getU8(lookMountFeet)) break;
                
                item.setGenericAttribute("podium_flags", static_cast<int64_t>(flags));
                item.setGenericAttribute("podium_direction", static_cast<int64_t>(direction));
                item.setGenericAttribute("podium_lookType", static_cast<int64_t>(lookType));
                item.setGenericAttribute("podium_lookHead", static_cast<int64_t>(lookHead));
                item.setGenericAttribute("podium_lookBody", static_cast<int64_t>(lookBody));
                item.setGenericAttribute("podium_lookLegs", static_cast<int64_t>(lookLegs));
                item.setGenericAttribute("podium_lookFeet", static_cast<int64_t>(lookFeet));
                item.setGenericAttribute("podium_lookAddon", static_cast<int64_t>(lookAddon));
                item.setGenericAttribute("podium_lookMount", static_cast<int64_t>(lookMount));
                item.setGenericAttribute("podium_lookMountHead", static_cast<int64_t>(lookMountHead));
                item.setGenericAttribute("podium_lookMountBody", static_cast<int64_t>(lookMountBody));
                item.setGenericAttribute("podium_lookMountLegs", static_cast<int64_t>(lookMountLegs));
                item.setGenericAttribute("podium_lookMountFeet", static_cast<int64_t>(lookMountFeet));
                break;
            }
            case OtbmAttribute::AttributeMap: {
                if (!parseAttributeMap(node, item)) {
                    spdlog::warn("Failed to parse attribute map");
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

bool OtbmItemParser::parseAttributeMap(BinaryNode* node, Domain::Item& item) {
    uint16_t count;
    if (!node->getU16(count)) return false;

    for (uint16_t i = 0; i < count; ++i) {
        std::string key;
        if (!node->getString(key)) return false;

        uint8_t type;
        if (!node->getU8(type)) return false;

        switch (type) {
            case 1: { // STRING
                std::string val;
                if (!node->getLongString(val)) return false;
                item.setGenericAttribute(key, val);
                break;
            }
            case 2: { // INTEGER (U32 signed)
                uint32_t val;
                if (!node->getU32(val)) return false;
                item.setGenericAttribute(key, static_cast<int64_t>(static_cast<int32_t>(val)));
                break;
            }
            case 3: { // FLOAT
                uint32_t val;
                if (!node->getU32(val)) return false;
                float f;
                std::memcpy(&f, &val, sizeof(f));
                item.setGenericAttribute(key, static_cast<double>(f));
                break;
            }
            case 4: { // DOUBLE
                uint64_t val;
                if (!node->getU64(val)) return false;
                double d;
                std::memcpy(&d, &val, sizeof(d));
                item.setGenericAttribute(key, d);
                break;
            }
            case 5: { // BOOLEAN
                uint8_t val;
                if (!node->getU8(val)) return false;
                item.setGenericAttribute(key, val != 0);
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

bool OtbmItemParser::parseItemChildren(BinaryNode* node, Domain::Item& item,
                                        OtbmVersion version, 
                                        Services::ClientDataService* client_data) {
    for (auto& child : node->children()) {
        uint8_t child_type;
        if (!child.getU8(child_type)) {
            continue;
        }
        
        if (child_type != static_cast<uint8_t>(OtbmNode::Item)) {
            continue;
        }
        
        auto child_item = parseItem(&child, version, client_data);
        if (child_item) {
            parseItemAttributes(&child, *child_item);
            parseItemChildren(&child, *child_item, version, client_data);
            item.addContainerItem(std::move(child_item));
        }
    }
    return true;
}

} // namespace IO
} // namespace MapEditor
