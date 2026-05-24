#include "StartupController.h"
#include "IO/Otbm/OtbmReader.h"
#include "Services/ClientSignatureDetector.h"
#include <chrono>
#include <iomanip>
#include <nfd.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace MapEditor {
namespace AppLogic {

StartupController::StartupController(
    UI::StartupDialog &dialog, MapOperationHandler &map_ops,
    Services::ConfigService &config, Services::ClientVersionRegistry &registry,
    Services::RecentLocationsService &recent_locations,
    AppStateManager &state_manager)
    : dialog_(dialog), map_ops_(map_ops), config_(config), registry_(registry),
      recent_locations_(recent_locations), state_manager_(state_manager) {
  spdlog::info("StartupController initialized");
}

void StartupController::update() {
  if (!dialog_.hasResult()) {
    return;
  }

  auto result = dialog_.consumeResult();

  switch (result.action) {
  case UI::StartupDialog::Action::SelectRecentMap:
    handleMapSelection(result.selected_path, result.selected_index);
    break;

  case UI::StartupDialog::Action::SelectClient:
    handleClientSelection(result.selected_version);
    break;

  case UI::StartupDialog::Action::BrowseMap:
    handleBrowseMap();
    break;

  case UI::StartupDialog::Action::BrowseSecMap:
    // Show SEC map modal in StartupDialog
    dialog_.showSecMapModal();
    break;

  case UI::StartupDialog::Action::NewMap:
    handleNewMapFlow();
    break;

  case UI::StartupDialog::Action::NewMapConfirmed:
    handleNewMapConfirmed(result.new_map_config);
    break;

  case UI::StartupDialog::Action::OpenSecMapConfirmed:
    handleOpenSecMapConfirmed(result.sec_map_folder, result.sec_map_version);
    break;

  case UI::StartupDialog::Action::ClientConfiguration:
    handleClientConfiguration();
    break;

  case UI::StartupDialog::Action::Preferences:
    handlePreferences();
    break;

  case UI::StartupDialog::Action::LoadMap:
    handleLoadMap();
    break;

  case UI::StartupDialog::Action::Exit:
    requestExit();
    break;

  default:
    break;
  }
}

std::vector<UI::RecentMapEntry> StartupController::getRecentMaps() const {
  std::vector<UI::RecentMapEntry> entries;

  auto recent_files = config_.getRecentFiles();

  for (const auto &file_path : recent_files) {
    std::filesystem::path const path(file_path);

    UI::RecentMapEntry entry;
    entry.path = path;
    entry.filename = path.filename().string();
    entry.exists = std::filesystem::exists(path);

    // Get last modified time
    if (entry.exists) {
      try {
        auto ftime = std::filesystem::last_write_time(path);
        auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        auto time = std::chrono::system_clock::to_time_t(sctp);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M");
        entry.last_modified = ss.str();
      } catch (...) {
        entry.last_modified = "Unknown";
      }
    } else {
      entry.last_modified = "File not found";
    }

    entries.push_back(entry);
  }

  return entries;
}

std::vector<uint32_t> StartupController::getRecentClients() const {
  std::vector<uint32_t> clients;

  // Get configured client versions
  auto all_versions = registry_.getAllVersions();
  for (const auto *version : all_versions) {
    if (!version->getClientPath().empty()) {
      clients.push_back(version->getVersion());
    }
  }

  // Limit to recent/commonly used (first 5)
  if (clients.size() > 5) {
    clients.resize(5);
  }

  return clients;
}

void StartupController::handleMapSelection(const std::filesystem::path &path,
                                           int index) {
  spdlog::info("Map selected: {}", path.string());

  selected_map_path_ = path;
  dialog_.setSelectedIndex(index);

  // Set map info
  UI::SelectedMapInfo map_info;
  map_info.name = path.stem().string();
  map_info.valid = true;

  // Read OTBM header to get actual map metadata
  if (std::filesystem::exists(path) && path.extension() == ".otbm") {
    auto otbm_result = IO::OtbmReader::readHeader(path);
    if (otbm_result.success) {
      const auto &ver = otbm_result.version;
      map_info.description = ver.description;
      map_info.width = ver.width;
      map_info.height = ver.height;

      // Per RME analysis: OTB Minor Version = ClientVersionID = otbId
      // This is the key for looking up the client configuration
      uint32_t client_version_id = ver.client_version_minor; // otbId
      auto *client_ver = registry_.getVersionByOtbVersion(client_version_id);
      if (client_ver) {
        map_info.client_version = client_ver->getVersion();
        spdlog::info("OTBM header: OTB Minor (otbId) {} -> Client Version {}",
                     client_version_id, client_ver->getVersion());
      } else {
        // Fallback: if OTB minor version looks like a direct version number (>=
        // 700), use it directly (some maps may store actual version)
        if (client_version_id >= 700) {
          map_info.client_version = client_version_id;
        } else {
          map_info.client_version = 0;
          spdlog::warn("No client version found for OTB Minor (otbId) {}",
                       client_version_id);
        }
      }

      map_info.house_file = ver.house_file;
      map_info.spawn_file = ver.spawn_file;
      map_info.otbm_version = ver.otbm_version;
      map_info.items_major_version = ver.client_version_major;
      map_info.items_minor_version =
          ver.client_version_minor; // This IS the otbId
      map_info.created = "";
      spdlog::info("OTBM header: {}x{}, client ver {}, OTBM v{}, OTB major {}, "
                   "OTB minor (otbId) {}",
                   ver.width, ver.height, map_info.client_version,
                   ver.otbm_version, ver.client_version_major,
                   ver.client_version_minor);
    } else {
      spdlog::warn("Failed to read OTBM header: {}", otbm_result.error);
    }
  } else if (std::filesystem::is_directory(path)) {
    // .sec folder - no OTBM metadata available
    spdlog::info("Directory selected (SEC map): {}", path.string());
  }

  dialog_.setSelectedMapInfo(map_info);

  // Attempt client auto-match
  handleClientAutoMatch(path);
}

void StartupController::handleClientAutoMatch(
    const std::filesystem::path &map_path) {
  spdlog::info("Attempting client auto-match for: {}", map_path.string());

  UI::ClientInfo client_info;
  Domain::ClientVersion  const*matched_version = nullptr;

  // Primary method: Use OTBM header's OTB Minor version (= otbId =
  // ClientVersionID) This is the RME-compatible approach
  const auto &map_info = dialog_.getSelectedMapInfo();
  if (map_info.valid && map_info.items_minor_version > 0) {
    uint32_t otb_id = map_info.items_minor_version;
    matched_version = registry_.getVersionByOtbVersion(otb_id);
    if (matched_version) {
      spdlog::info("Client matched via OTBM otbId {}: version {}", otb_id,
                   matched_version->getVersion());
    }
  }

  // Fallback: Try to detect client version from signatures in map folder
  if (!matched_version) {
    auto parent_path = map_path.parent_path();
    uint32_t detected_version =
        Services::ClientSignatureDetector::detectFromFolder(
            parent_path, registry_.getVersionsMap());
    if (detected_version > 0) {
      matched_version = registry_.getVersion(detected_version);
      if (matched_version) {
        spdlog::info("Client matched via signatures: version {}",
                     detected_version);
      }
    }
  }

  if (matched_version) {
    uint32_t version_num = matched_version->getVersion();
    matched_client_version_ = version_num;
    client_info.version = version_num;
    client_info.version_string = "Tibia " + std::to_string(version_num / 100) +
                                 "." + std::to_string(version_num % 100);

    // Format signatures as hex strings
    std::ostringstream dat_ss, spr_ss;
    dat_ss << std::uppercase << std::hex << matched_version->getDatSignature();
    spr_ss << std::uppercase << std::hex << matched_version->getSprSignature();
    client_info.dat_signature = dat_ss.str();
    client_info.spr_signature = spr_ss.str();

    // Populate version comparison fields
    client_info.otbm_version = matched_version->getOtbmVersion();
    client_info.items_major_version = matched_version->getOtbMajor();
    client_info.items_minor_version = matched_version->getOtbVersion(); // otbId

    // Populate new fields
    client_info.client_name = matched_version->getName();
    client_info.data_directory = matched_version->getDataDirectory();
    client_info.description = matched_version->getDescription();

    // Determine match status by comparing with map info
    // CRITICAL: Items Major and Minor MUST match. OTBM version mismatch is just
    // a warning.
    const auto &map_info = dialog_.getSelectedMapInfo();
    bool const otbm_match = (client_info.otbm_version == map_info.otbm_version);
    bool const major_match =
        (client_info.items_major_version == map_info.items_major_version);
    bool const minor_match =
        (client_info.items_minor_version == map_info.items_minor_version);

    // Items versions are critical - they determine item ID mapping
    bool items_compatible = major_match && minor_match;

    if (items_compatible) {
      client_info.signatures_match = true;
      if (otbm_match) {
        client_info.status = "Compatible";
      } else {
        client_info.status = "Compatible (OTBM format differs)";
      }
      dialog_.setClientInfo(client_info);
      dialog_.setSignatureMismatch(false, "");
      dialog_.setLoadEnabled(true);
    } else {
      client_info.signatures_match = false;
      client_info.status = "Items Version Mismatch";
      dialog_.setClientInfo(client_info);
      dialog_.setSignatureMismatch(
          true, "Items version mismatch! Map requires Items " +
                    std::to_string(map_info.items_major_version) + "." +
                    std::to_string(map_info.items_minor_version) +
                    " but client provides " +
                    std::to_string(client_info.items_major_version) + "." +
                    std::to_string(client_info.items_minor_version) +
                    ". Toggle 'Ignore signatures' to force load.");
      dialog_.setLoadEnabled(false); // Block loading until ignore toggled
    }

    spdlog::info(
        "Client auto-matched: version {}, items compatible: {}, status: {}",
        version_num, items_compatible, client_info.status);
  } else {
    // No match - show warning
    client_info.version = 0;
    client_info.version_string = "Unknown";
    client_info.signatures_match = false;
    client_info.status = "Not Found";

    dialog_.setClientInfo(client_info);
    dialog_.setSignatureMismatch(
        true,
        "Could not auto-detect client version. "
        "Please configure a client manually or check 'Ignore signatures'.");
    dialog_.setLoadEnabled(false);

    spdlog::warn("Client auto-match failed for: {}", map_path.string());
  }
}

void StartupController::handleClientSelection(uint32_t version) {
  spdlog::info("Manual client selection: version {}", version);

  auto *selected_version = registry_.getVersion(version);
  if (!selected_version) {
    spdlog::warn("Selected client version {} not found in registry", version);
    return;
  }

  matched_client_version_ = version;

  // Populate ClientInfo from selected version
  UI::ClientInfo client_info;
  client_info.version = version;
  client_info.version_string = "Tibia " + std::to_string(version / 100) + "." +
                               std::to_string(version % 100);

  // Format signatures as hex strings
  std::ostringstream dat_ss, spr_ss;
  dat_ss << std::uppercase << std::hex << selected_version->getDatSignature();
  spr_ss << std::uppercase << std::hex << selected_version->getSprSignature();
  client_info.dat_signature = dat_ss.str();
  client_info.spr_signature = spr_ss.str();

  // Populate version comparison fields
  client_info.otbm_version = selected_version->getOtbmVersion();
  client_info.items_major_version = selected_version->getOtbMajor();
  client_info.items_minor_version = selected_version->getOtbVersion();

  // Populate new fields
  client_info.client_name = selected_version->getName();
  client_info.data_directory = selected_version->getDataDirectory();
  client_info.description = selected_version->getDescription();

  // Determine match status with currently selected map (if any)
  const auto &map_info = dialog_.getSelectedMapInfo();
  if (map_info.valid) {
    bool const major_match =
        (client_info.items_major_version == map_info.items_major_version);
    bool const minor_match =
        (client_info.items_minor_version == map_info.items_minor_version);
    bool const items_compatible = major_match && minor_match;

    if (items_compatible) {
      client_info.signatures_match = true;
      client_info.status = "Compatible";
      dialog_.setSignatureMismatch(false, "");
      dialog_.setLoadEnabled(true);
    } else {
      client_info.signatures_match = false;
      client_info.status = "Items Version Mismatch";
      dialog_.setSignatureMismatch(true,
                                   "Items version mismatch with selected map. "
                                   "Toggle 'Ignore signatures' to force load.");
      dialog_.setLoadEnabled(false);
    }
  } else {
    // No map selected yet
    client_info.signatures_match = true;
    client_info.status = "Ready";
    dialog_.setSignatureMismatch(false, "");
    dialog_.setLoadEnabled(false); // Can't load without a map
  }

  dialog_.setClientInfo(client_info);
  dialog_.setClientNotConfigured(selected_version->getClientPath().empty());
  spdlog::info("Client {} selected, status: {}", version, client_info.status);
}

void StartupController::handleBrowseMap() {
  spdlog::info("Opening file dialog for map selection");

  NFD::UniquePath outPath;
  nfdfilteritem_t filterItem[1] = {{"Map Files", "otbm,map"}};
  nfdresult_t const result = NFD::OpenDialog(outPath, filterItem, 1);

  if (result == NFD_OKAY) {
    std::filesystem::path const path(outPath.get());

    // Add to recent files
    config_.addRecentFile(path.string());

    // Select the map
    handleMapSelection(path, 0);
  }
}

void StartupController::handleBrowseSecMap() {
  spdlog::info("Opening folder dialog for .sec map selection");

  NFD::UniquePath outPath;
  nfdresult_t const result = NFD::PickFolder(outPath);

  if (result == NFD_OKAY) {
    std::filesystem::path const path(outPath.get());

    // Validate .sec folder (should contain .sec files)
    bool has_sec_files = false;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.path().extension() == ".sec") {
        has_sec_files = true;
        break;
      }
    }

    if (has_sec_files) {
      // Add to recent files
      config_.addRecentFile(path.string());

      // Select the map
      handleMapSelection(path, 0);
    } else {
      spdlog::warn("Selected folder does not contain .sec files: {}",
                   path.string());
    }
  }
}

void StartupController::handleNewMapFlow() {
  spdlog::info("Opening new map modal");
  
  // Just show the modal in StartupDialog - no state transition needed
  dialog_.showNewMapModal();
}

void StartupController::handleNewMapConfirmed(const UI::NewMapPanel::State& config) {
  spdlog::info("Creating new map: {} ({}x{}) for version {}", config.map_name, 
               config.map_width, config.map_height, config.selected_version);
  
  // Save client path to version registry if provided
  if (config.selected_version > 0 && !config.client_path.empty()) {
    registry_.setClientPath(config.selected_version, config.client_path);
    registry_.savePathsToConfig(config_);
    config_.setLastClientVersion(std::to_string(config.selected_version));
    config_.save();
  }
  
  // Create new map directly via MapOperationHandler
  map_ops_.handleNewMapDirect(config.map_name, config.map_width, 
                               config.map_height, config.selected_version);
}

void StartupController::handleOpenSecMapConfirmed(
    const std::filesystem::path& folder, uint32_t version) {
  spdlog::info("Opening SEC map: {} version {}", folder.string(), version);
  
  // Save version to config
  config_.setLastClientVersion(std::to_string(version));
  config_.save();
  
  // Load SEC map directly via MapOperationHandler
  map_ops_.handleOpenSecMapDirect(folder, version);
}

void StartupController::handleLoadMap() {
  spdlog::info("Loading map: {}", selected_map_path_.string());

  // Load via MapOperationHandler - now loads directly, no ProjectConfig state needed
  map_ops_.handleOpenRecentMap(selected_map_path_, matched_client_version_);
}

void StartupController::handleClientConfiguration() {
  spdlog::info("Opening client configuration dialog");

  dialog_.getClientConfigDialog().open(registry_, config_);
}

void StartupController::handlePreferences() {
  spdlog::info("Opening preferences dialog");

  if (on_open_preferences_) {
    on_open_preferences_();
  } else {
    spdlog::warn("Preferences callback not set");
  }
}

void StartupController::requestExit() {
  spdlog::info("Exit requested");
  exit_requested_ = true;
}

} // namespace AppLogic
} // namespace MapEditor
