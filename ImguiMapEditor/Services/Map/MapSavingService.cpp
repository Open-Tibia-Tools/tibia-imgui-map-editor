#include "MapSavingService.h"
#include "IO/Otbm/OtbmWriter.h"
#include "IO/HouseXmlWriter.h"
#include "IO/SpawnXmlWriter.h"
#include "Domain/ChunkedMap.h"
#include "Services/ClientDataService.h"
namespace MapEditor::Services {

MapSavingService::MapSavingService(ClientDataService* client_data)
    : client_data_(client_data) {
}

MapSaveResult MapSavingService::save(
    const std::filesystem::path& path,
    const Domain::ChunkedMap& map,
    SaveProgressCallback progress
) {
    MapSaveResult result;
    
    // Write OTBM
    auto otbm_result = IO::OtbmWriter::write(
        path,
        map,
        IO::OtbmVersion::V2,
        client_data_,
        IO::OtbmConversionMode::None,  // No ID conversion for normal save
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
        std::string const house_file = map.getHouseFile();
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
        std::string const spawn_file = map.getSpawnFile();
        if (!spawn_file.empty()) {
            if (progress) progress(95, "Writing spawns...");
            
            auto spawn_path = path.parent_path() / spawn_file;
            if (!IO::SpawnXmlWriter::write(spawn_path, map)) {
                result.error = "Failed to write spawn file";
                return result;
            }
        }
    }
    
    if (progress) progress(100, "Complete");
    
    result.success = true;
    return result;
}

} // namespace MapEditor::Services
