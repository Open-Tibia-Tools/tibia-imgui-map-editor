#pragma once
#include "IO/Otbm/OtbmWriter.h"
#include "Domain/OtbmDataTypes.h"
#include "Services/OtbmSettings.h"
#include <filesystem>
#include <functional>

namespace MapEditor {
namespace Domain {
    class ChunkedMap;
}
}

namespace MapEditor::Services {

class ClientDataService;

/**
 * Progress callback for map saving
 */
using SaveProgressCallback = std::function<void(int percent, const std::string& status)>;

/**
 * Result of map saving operation
 */
struct MapSaveResult {
    bool success = false;
    std::string error;
    
    size_t tiles_saved = 0;
    size_t items_saved = 0;
};

/**
 * Service for saving maps and associated files.
 * Orchestrates OTBM, house, spawn, and waypoint writing.
 */
class MapSavingService {
public:
    explicit MapSavingService(ClientDataService* client_data = nullptr,
                             const OtbmSettings* otbm_settings = nullptr);
    
    /**
     * Save map to file.
     * Automatically saves associated house/spawn files if configured.
     * @param path Output .otbm path
     * @param map Map to save
     * @param progress Progress callback (optional)
     * @return Save result
     */
    MapSaveResult save(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map,
        SaveProgressCallback progress = nullptr
    );
    
    /**
     * Set whether to save house file.
     */
    void setSaveHouses(bool save) { save_houses_ = save; }
    
    /**
     * Set whether to save spawn file.
     */
    void setSaveSpawns(bool save) { save_spawns_ = save; }
    
private:
    ClientDataService* client_data_;
    const OtbmSettings* otbm_settings_;
    bool save_houses_ = true;
    bool save_spawns_ = true;
};

} // namespace MapEditor::Services
