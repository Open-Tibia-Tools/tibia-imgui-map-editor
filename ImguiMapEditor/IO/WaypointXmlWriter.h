#pragma once
#include "Domain/ChunkedMap.h"
#include <filesystem>

namespace MapEditor::IO {

/**
 * Writes waypoint data to XML format.
 */
class WaypointXmlWriter {
public:
    /**
     * Write waypoints.xml file.
     * @param path Output file path
     * @param map Map containing waypoint data
     * @return true on success
     */
    static bool write(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map
    );

private:
    WaypointXmlWriter() = delete;
};

} // namespace MapEditor::IO
