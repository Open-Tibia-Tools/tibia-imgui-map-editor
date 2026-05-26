#pragma once
#include "Domain/ChunkedMap.h"
#include <filesystem>
#include <string>

namespace MapEditor {
namespace IO {

class WaypointXmlReader {
public:
    struct Result {
        bool success = false;
        std::string error;
        int waypoints_loaded = 0;
    };

    static Result read(const std::filesystem::path& path, Domain::ChunkedMap& map);
};

} // namespace IO
} // namespace MapEditor
