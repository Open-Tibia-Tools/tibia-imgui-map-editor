#include "WaypointXmlReader.h"
#include "XmlUtils.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace IO {

WaypointXmlReader::Result WaypointXmlReader::read(const std::filesystem::path& path, Domain::ChunkedMap& map) {
    Result result;
    pugi::xml_document doc;

    pugi::xml_node root = XmlUtils::loadXmlFile(path, "waypoints", doc, result.error);
    if (!root) {
        return result;
    }

    for (pugi::xml_node wpNode : root.children("waypoint")) {
        std::string name = wpNode.attribute("name").as_string();
        if (name.empty()) {
            spdlog::warn("Waypoint with empty name at {}, skipping", path.string());
            continue;
        }

        Domain::Position pos;
        pos.x = wpNode.attribute("x").as_int();
        pos.y = wpNode.attribute("y").as_int();
        pos.z = wpNode.attribute("z").as_int();

        if (pos.x == 0 && pos.y == 0) {
            spdlog::warn("Waypoint '{}' has invalid position (0,0), skipping", name);
            continue;
        }

        if (map.getWaypointAt(pos)) {
            continue;
        }

        map.addWaypoint(name, pos);
        result.waypoints_loaded++;
    }

    result.success = true;
    spdlog::info("Loaded {} waypoints from {}", result.waypoints_loaded, path.string());
    return result;
}

} // namespace IO
} // namespace MapEditor
