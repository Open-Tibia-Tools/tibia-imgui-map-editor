#pragma once
#include "Domain/ChunkedMap.h"
#include "OtbmReader.h"  // For OtbmNode, OtbmAttribute, OtbmVersion enums
#include <filesystem>
#include <functional>
#include <string>

namespace MapEditor {
namespace Services {
    class ClientDataService;
}
namespace IO {

/**
 * Progress callback for OTBM writing
 */
using OtbmWriteProgressCallback = std::function<void(int percent, const std::string& status)>;

/**
 * ID conversion mode for OTBM writing
 * Conversion happens at write-time without modifying Item objects
 */
enum class OtbmConversionMode {
    None,       // Write IDs as-is (default)
    ToClient,   // Convert server_id -> client_id during write
    ToServer    // Convert client_id -> server_id during write
};

/**
 * Result of OTBM writing operation
 */
struct OtbmWriteResult {
    bool success = false;
    std::string error;
    
    size_t tiles_written = 0;
    size_t items_written = 0;
    size_t items_converted = 0;
    size_t items_skipped = 0;
};

/**
 * OTBM map file writer.
 * Writes maps in OTBM binary format compatible with OT servers.
 */
class OtbmWriter {
public:
    /**
     * Write map to OTBM file.
     * @param path Output file path (.otbm)
     * @param map Map to write
     * @param version OTBM format version
     * @param client_data Client data for ID conversion (required if conversion_mode != None)
     * @param conversion_mode ID conversion mode (default: None)
     * @param progress Progress callback (optional)
     * @return Write result with statistics
     */
    static OtbmWriteResult write(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map,
        OtbmVersion version = OtbmVersion::V1,
        Services::ClientDataService* client_data = nullptr,
        OtbmConversionMode conversion_mode = OtbmConversionMode::None,
        OtbmWriteProgressCallback progress = nullptr
    );
    
    /**
     * Write associated house file.
     */
    static bool writeHouses(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map
    );
    
    /**
     * Write associated spawn file.
     */
    static bool writeSpawns(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map
    );
    
    /**
     * Write associated waypoints file.
     */
    static bool writeWaypoints(
        const std::filesystem::path& path,
        const Domain::ChunkedMap& map
    );

private:
    OtbmWriter() = delete;
};

} // namespace IO
} // namespace MapEditor
