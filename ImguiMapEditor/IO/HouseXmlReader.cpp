#include "HouseXmlReader.h"
#include "XmlUtils.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace IO {

HouseXmlReader::Result HouseXmlReader::read(const std::filesystem::path& path, Domain::ChunkedMap& map) {
    Result result;
    pugi::xml_document doc;

    pugi::xml_node root = XmlUtils::loadXmlFile(path, "houses", doc, result.error);
    if (!root) {
        return result;
    }

    for (pugi::xml_node houseNode : root.children("house")) {
        uint32_t id = houseNode.attribute("houseid").as_uint();
        if (id == 0) continue;

        uint32_t town_id = houseNode.attribute("townid").as_uint();
        if (town_id == 0) {
            spdlog::warn("House {} has no townid, skipping XML entry", id);
            continue;
        }

        auto* existing = map.getHouse(id);
        if (existing) {
            existing->name = houseNode.attribute("name").as_string();
            existing->rent = houseNode.attribute("rent").as_uint();
            existing->town_id = town_id;
            existing->is_guildhall = houseNode.attribute("guildhall").as_bool();
            existing->entry_position.x = houseNode.attribute("entryx").as_int();
            existing->entry_position.y = houseNode.attribute("entryy").as_int();
            existing->entry_position.z = houseNode.attribute("entryz").as_int();
        } else {
            auto house = std::make_unique<Domain::House>(id);
            house->name = houseNode.attribute("name").as_string();
            house->rent = houseNode.attribute("rent").as_uint();
            house->town_id = town_id;
            house->is_guildhall = houseNode.attribute("guildhall").as_bool();
            house->entry_position.x = houseNode.attribute("entryx").as_int();
            house->entry_position.y = houseNode.attribute("entryy").as_int();
            house->entry_position.z = houseNode.attribute("entryz").as_int();
            map.addHouse(std::move(house));
        }

        result.houses_loaded++;
    }

    result.success = true;
    return result;
}

} // namespace IO
} // namespace MapEditor
