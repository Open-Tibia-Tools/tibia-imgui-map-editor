#pragma once
#include "IO/Otbm/OtbmReader.h"
#include <filesystem>
#include <string>
#include <cstdint>

namespace MapEditor {
namespace Services {

class ClientVersionRegistry;

/**
 * Validates client paths and detects client versions.
 * 
 * Single responsibility: client version detection and path validation.
 * Extracted from ProjectConfigDialog to separate business logic from UI.
 */
class ClientVersionValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        uint32_t detected_version = 0;
    };
    
    explicit ClientVersionValidator(ClientVersionRegistry& registry);
    
    /**
     * Validate that a client path contains required files.
     * Checks for Tibia.dat, Tibia.spr, and items.otb
     */
    ValidationResult validateClientPath(const std::filesystem::path& client_path) const;
    
    /**
     * Validate client path and check version compatibility with map.
     */
    ValidationResult validateWithMapVersion(
        const std::filesystem::path& client_path,
        uint32_t map_otb_version,
        bool skip_validation = false) const;
    
    /**
     * Detect client version from folder signatures.
     */
    uint32_t detectVersion(const std::filesystem::path& client_path) const;
    
    /**
     * Read OTBM map header to get version info.
     */
    static IO::OtbmVersionInfo readMapHeader(const std::filesystem::path& map_path) ;
    
    /**
     * Check if map header read was successful.
     */
    static bool isMapHeaderValid(const std::filesystem::path& map_path) ;

private:
    ClientVersionRegistry& registry_;
};

} // namespace Services
} // namespace MapEditor
