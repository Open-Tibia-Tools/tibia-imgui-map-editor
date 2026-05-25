#include "ClientVersionValidator.h"
#include "ClientSignatureDetector.h"
#include "ClientVersionRegistry.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

ClientVersionValidator::ClientVersionValidator(ClientVersionRegistry& registry)
    : registry_(registry) {}

ClientVersionValidator::ValidationResult ClientVersionValidator::validateClientPath(
    const std::filesystem::path& client_path) const {
    
    ValidationResult result;
    
    if (client_path.empty()) {
        result.error_message = "Select a client data folder";
        return result;
    }
    
    if (!std::filesystem::exists(client_path)) {
        result.error_message = "Folder does not exist";
        return result;
    }
    
    auto dat_path = client_path / "Tibia.dat";
    auto spr_path = client_path / "Tibia.spr";
    auto otb_path = client_path / "items.otb";
    auto srv_path = client_path / "items.srv";  // Ancient 7.x format fallback
    
    std::vector<std::string> missing;
    if (!std::filesystem::exists(dat_path)) missing.push_back("Tibia.dat");
    if (!std::filesystem::exists(spr_path)) missing.push_back("Tibia.spr");
    // Accept either items.otb OR items.srv (ancient format)
    if (!std::filesystem::exists(otb_path) && !std::filesystem::exists(srv_path)) {
        missing.push_back("items.otb");
    }
    
    if (!missing.empty()) {
        result.error_message = "Missing: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) result.error_message += ", ";
            result.error_message += missing[i];
        }
        return result;
    }
    
    result.detected_version = detectVersion(client_path);
    result.is_valid = true;
    return result;
}

ClientVersionValidator::ValidationResult ClientVersionValidator::validateWithMapVersion(
    const std::filesystem::path& client_path,
    uint32_t map_otb_version,
    bool skip_validation) const {
    
    auto result = validateClientPath(client_path);
    if (!result.is_valid) {
        return result;
    }
    
    if (skip_validation) {
        return result;
    }
    
    // Get client version from map's OTB version
    uint32_t map_client_version = 0;
    if (auto* matched = registry_.findBestByVersion(result.detected_version)) {
        // Verify the matched client's OTB version matches the map's OTB version
        if (matched->getOtbVersion() == map_otb_version) {
            map_client_version = matched->getVersion();
        }
    }
    
    if (map_client_version > 0 && result.detected_version > 0 && 
        map_client_version != result.detected_version) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Version mismatch: Map is %u.%02u, Client is %u.%02u",
                 map_client_version / 100, map_client_version % 100,
                 result.detected_version / 100, result.detected_version % 100);
        result.error_message = buf;
        result.is_valid = false;
    }
    
    return result;
}

uint32_t ClientVersionValidator::detectVersion(const std::filesystem::path& client_path) const {
    if (client_path.empty()) {
        return 0;
    }
    return ClientSignatureDetector::detectFromFolder(client_path, registry_.getVersionsMap());
}

IO::OtbmVersionInfo ClientVersionValidator::readMapHeader(
    const std::filesystem::path& map_path) const {
    
    if (map_path.empty() || !std::filesystem::exists(map_path)) {
        return {};
    }
    
    auto result = IO::OtbmReader::readHeader(map_path);
    if (result.success) {
        spdlog::info("OtbmReader: Header read successfully. Version: {}, Size: {}x{}, Client: {}.{}",
                     result.version.otbm_version, result.version.width, result.version.height,
                     result.version.client_version / 100, result.version.client_version % 100);
        return result.version;
    }
    return {};
}

bool ClientVersionValidator::isMapHeaderValid(const std::filesystem::path& map_path) const {
    if (map_path.empty() || !std::filesystem::exists(map_path)) {
        return false;
    }
    auto result = IO::OtbmReader::readHeader(map_path);
    return result.success;
}

} // namespace Services
} // namespace MapEditor
