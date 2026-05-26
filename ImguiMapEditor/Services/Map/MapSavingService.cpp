#include "MapSavingService.h"
#include "IO/Otbm/OtbmWriter.h"
#include "IO/HouseXmlWriter.h"
#include "IO/SpawnXmlWriter.h"
#include "IO/WaypointXmlWriter.h"
#include "Domain/ChunkedMap.h"
#include "Services/ClientDataService.h"
#include "Services/OtbmSettings.h"
#include <spdlog/spdlog.h>
namespace MapEditor::Services {

MapSavingService::MapSavingService(ClientDataService* client_data,
                                   const OtbmSettings* otbm_settings)
    : client_data_(client_data), otbm_settings_(otbm_settings) {
}

MapSaveResult MapSavingService::save(
    const std::filesystem::path& path,
    const Domain::ChunkedMap& map,
    SaveProgressCallback progress
) {
    MapSaveResult result;
    
    // Write OTBM
    auto raw_version = map.getVersion().otbm_version;
    IO::OtbmVersion version = IO::OtbmVersion::V4;
    if (raw_version <= static_cast<uint32_t>(IO::OtbmVersion::V4)) {
        version = static_cast<IO::OtbmVersion>(raw_version);
    } else {
        spdlog::warn("Invalid OTBM version {}, clamping to V4", raw_version);
    }

    Domain::OtbmWriteTarget waypoint_target = Domain::OtbmWriteTarget::Otbm;
    if (otbm_settings_) {
        waypoint_target = otbm_settings_->waypoint_write;
    }

    auto otbm_result = IO::OtbmWriter::write(
        path,
        map,
        version,
        client_data_,
        IO::OtbmConversionMode::None,
        waypoint_target,
        [&progress](int percent, const std::string& status) {
            if (progress) {
                // OTBM is 0-80% of total progress
                progress(percent * 80 / 100, status);
            }
        }
    );
    
    if (!otbm_result.success) {
        result.error = otbm_result.error;
        return result;
    }
    
    result.tiles_saved = otbm_result.tiles_written;
    result.items_saved = otbm_result.items_written;
    
    // Write houses
    if (save_houses_) {
        std::string house_file = map.getHouseFile();
        if (!house_file.empty()) {
            if (progress) progress(85, "Writing houses...");
            
            auto house_path = path.parent_path() / house_file;
            if (!IO::HouseXmlWriter::write(house_path, map)) {
                result.error = "Failed to write house file";
                return result;
            }
        }
    }
    
    // Write spawns
    if (save_spawns_) {
        std::string spawn_file = map.getSpawnFile();
        if (!spawn_file.empty()) {
            if (progress) progress(95, "Writing spawns...");
            
            auto spawn_path = path.parent_path() / spawn_file;
            if (!IO::SpawnXmlWriter::write(spawn_path, map)) {
                result.error = "Failed to write spawn file";
                return result;
            }
        }
    }
    
    // Write waypoints (external XML) — only when XML target
    if (waypoint_target == Domain::OtbmWriteTarget::Xml) {
        const auto& waypoints = map.getWaypoints();
        if (!waypoints.empty()) {
            if (progress) progress(97, "Writing waypoints...");

            auto waypoint_path = path.parent_path() / (path.stem().string() + "-waypoints.xml");
            if (!IO::WaypointXmlWriter::write(waypoint_path, map)) {
                result.error = "Failed to write waypoint file";
                return result;
            }
        }
    }
    
    if (progress) progress(100, "Complete");
    
    result.success = true;
    return result;
}

} // namespace MapEditor::Services
