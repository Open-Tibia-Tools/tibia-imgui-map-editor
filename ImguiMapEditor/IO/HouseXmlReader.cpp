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

        auto* house = map.getHouse(id);
        if (!house) {
            auto new_house = std::make_unique<Domain::House>(id);
            house = new_house.get();
            map.addHouse(std::move(new_house));
        }

        house->name = houseNode.attribute("name").as_string();
        house->rent = houseNode.attribute("rent").as_uint();
        house->town_id = town_id;
        house->is_guildhall = houseNode.attribute("guildhall").as_bool();
        house->entry_position.x = houseNode.attribute("entryx").as_int();
        house->entry_position.y = houseNode.attribute("entryy").as_int();
        house->entry_position.z = houseNode.attribute("entryz").as_int();

        result.houses_loaded++;
    }

    result.success = true;
    return result;
}

} // namespace IO
} // namespace MapEditor
