#include "WaypointXmlWriter.h"
#include <fstream>

namespace MapEditor::IO {

bool WaypointXmlWriter::write(
    const std::filesystem::path& path,
    const Domain::ChunkedMap& map
) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<waypoints>\n";
    
    const auto& waypoints = map.getWaypoints();
    for (const auto& wp : waypoints) {
        file << "\t<waypoint name=\"" << wp.name << "\"";
        file << " x=\"" << wp.position.x << "\"";
        file << " y=\"" << wp.position.y << "\"";
        file << " z=\"" << static_cast<int>(wp.position.z) << "\"/>\n";
    }
    
    file << "</waypoints>\n";
    file.close();
    
    return true;
}

} // namespace MapEditor::IO
