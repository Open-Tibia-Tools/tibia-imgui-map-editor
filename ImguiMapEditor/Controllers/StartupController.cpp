#include "StartupController.h"
#include "Presentation/Dialogs/ClientConfigurationController.h"
#include "IO/Otbm/OtbmReader.h"
#include "Services/ClientSignatureDetector.h"
#include <chrono>
#include <cctype>
#include <iomanip>
#include <nfd.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace MapEditor {
namespace AppLogic {

StartupController::~StartupController() = default;

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
    handleClientSelection(result.selected_client_index);
    break;

  case UI::StartupDialog::Action::BrowseMap:
    handleBrowseMap();
    break;

  case UI::StartupDialog::Action::BrowseSecMap:
    handleBrowseSecMap();
    break;

  case UI::StartupDialog::Action::NewMap:
    handleNewMapFlow();
    break;

  case UI::StartupDialog::Action::NewMapConfirmed:
    handleNewMapConfirmed(result.new_map_config);
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
    std::filesystem::path path(file_path);

    UI::RecentMapEntry entry;
    entry.path = path;

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    bool is_directory = std::filesystem::is_directory(status);
    if (is_directory) {
      entry.filename = std::format("..\\{}\\sec", path.filename().string());
    } else {
      entry.filename = path.filename().string();
    }
    entry.exists = std::filesystem::exists(status);

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
      auto *client_ver = registry_.findBestMatch(client_version_id, ver.client_version_major);
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
    // SEC folder - no OTBM metadata available
    map_info.name = std::format("..\\{}\\sec", path.filename().string());
    map_info.description = ".SEC map file folder.";
    map_info.valid = true;
    map_info.width = 0;
    map_info.height = 0;
    map_info.client_version = 0;
    map_info.otbm_version = 0;
    map_info.items_major_version = 0;
    map_info.items_minor_version = 0;
    spdlog::info("SEC map directory selected: {}", path.string());
  }

  dialog_.setSelectedMapInfo(map_info);

  // Attempt client auto-match
  handleClientAutoMatch(path);
}

void StartupController::handleClientAutoMatch(
    const std::filesystem::path &map_path) {
  spdlog::info("Attempting client auto-match for: {}", map_path.string());

  UI::ClientInfo client_info;
  const Domain::ClientVersion *matched_version = nullptr;

  matched_client_index_ = 0;
  bool is_sec = std::filesystem::is_directory(map_path);

  const auto &map_info = dialog_.getSelectedMapInfo();

  if (map_info.valid && map_info.items_minor_version > 0 && !is_sec) {
    uint32_t otb_minor = map_info.items_minor_version;
    uint32_t items_major = map_info.items_major_version;
    matched_version = registry_.findBestMatch(otb_minor, items_major);
    if (matched_version) {
      matched_client_index_ = matched_version->getIndex();
      spdlog::info("Client matched via OTBM otbId {}.{}: index {}, version {}",
                   otb_minor, items_major, matched_client_index_,
                   matched_version->getVersion());
    }
  }

  // SEC map: find best validated SRV client
  if (!matched_version && is_sec) {
    for (const auto* cv : registry_.getAllVersions()) {
      if (cv->getDataSource() == Domain::ItemDataSource::SRV &&
          !cv->getClientPath().empty() && cv->validateFiles()) {
        if (!matched_version || cv->getIndex() < matched_version->getIndex()) {
          matched_version = cv;
        }
      }
    }
    if (matched_version) {
      matched_client_index_ = matched_version->getIndex();
      spdlog::info("SEC map matched SRV client index {}", matched_client_index_);
    }
  }

  // Fallback: signature detection (skip for SEC — not applicable)
  if (!matched_version && !is_sec) {
    auto parent_path = map_path.parent_path();
    uint32_t detected_version =
        Services::ClientSignatureDetector::detectFromFolder(
            parent_path, registry_.getVersionsMap());
    if (detected_version > 0) {
      matched_version = registry_.findBestByVersion(detected_version);
      if (matched_version) {
        matched_client_index_ = matched_version->getIndex();
        spdlog::info("Client matched via signatures: version {}",
                     detected_version);
      }
    }
  }

  if (matched_version) {
    uint32_t version_num = matched_version->getVersion();
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

    // SEC maps don't use OTB — OTBM items-version compatibility is irrelevant
    if (is_sec) {
      client_info.status = "Compatible (SEC)";
      dialog_.setClientInfo(client_info);
      dialog_.setSignatureMismatch(false, "");
      dialog_.setLoadEnabled(true);
      spdlog::info("Client auto-matched for SEC: version {}, status: {}",
                   version_num, client_info.status);
    } else {
    bool otbm_match = (client_info.otbm_version == map_info.otbm_version);
    bool major_match =
        (client_info.items_major_version == map_info.items_major_version);
    bool minor_match =
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
    }

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

void StartupController::handleClientSelection(uint32_t index) {
  spdlog::info("Manual client selection: index {}", index);

  auto *selected_version = registry_.getVersion(index);
  if (!selected_version) {
    spdlog::warn("Selected client index {} not found in registry", index);
    return;
  }

  matched_client_index_ = index;

  // Populate ClientInfo from selected version
  UI::ClientInfo client_info;
  client_info.version = selected_version->getVersion();
  client_info.version_string = "Tibia " + std::to_string(selected_version->getVersion() / 100) + "." +
                               std::to_string(selected_version->getVersion() % 100);

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
    bool major_match =
        (client_info.items_major_version == map_info.items_major_version);
    bool minor_match =
        (client_info.items_minor_version == map_info.items_minor_version);
    bool items_compatible = major_match && minor_match;

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
  spdlog::info("Client {} selected, status: {}", selected_version->getVersion(), client_info.status);
}

void StartupController::handleBrowseMap() {
  spdlog::info("Opening file dialog for map selection");

  NFD::UniquePath outPath;
  nfdfilteritem_t filterItem[1] = {{"Map Files", "otbm,map"}};
  nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1);

  if (result == NFD_OKAY) {
    std::filesystem::path path(outPath.get());

    // Add to recent files
    config_.addRecentFile(path.string());

    // Select the map
    handleMapSelection(path, 0);
    recent_locations_.addRecentMap(path, matched_client_index_);
  }
}

void StartupController::handleBrowseSecMap() {
  spdlog::info("Opening folder dialog for .sec map selection");

  NFD::UniquePath outPath;
  nfdresult_t result = NFD::PickFolder(outPath);

  if (result == NFD_OKAY) {
    std::filesystem::path path(outPath.get());

    // Validate .sec folder (should contain .sec files — case-insensitive)
    bool has_sec_files = false;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
      std::string ext = entry.path().extension().string();
      for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (ext == ".sec") {
        has_sec_files = true;
        break;
      }
    }

    if (ec) {
      spdlog::error("Failed to read SEC folder: {}", ec.message());

      UI::SelectedMapInfo map_info;
      map_info.name = path.filename().string();
      map_info.valid = false;
      map_info.description = std::format("Folder unreadable: {}", ec.message());
      dialog_.setSelectedMapInfo(map_info);
    } else if (has_sec_files) {
      // Add to recent files
      config_.addRecentFile(path.string());

      // Select and set SEC-specific info
      selectSECMapFolder(path);
      recent_locations_.addRecentMap(path, matched_client_index_);
    } else {
      spdlog::warn("Selected folder does not contain .sec files: {}",
                   path.string());

      UI::SelectedMapInfo map_info;
      map_info.name = path.filename().string();
      map_info.valid = false;
      map_info.description = "No .sec files found in this folder.";
      dialog_.setSelectedMapInfo(map_info);
    }
  }
}

void StartupController::selectSECMapFolder(const std::filesystem::path &folder) {
  selected_map_path_ = folder;

  UI::SelectedMapInfo map_info;
  map_info.name = std::format("..\\{}\\sec", folder.filename().string());
  map_info.valid = true;
  map_info.width = 0;
  map_info.height = 0;
  map_info.client_version = 0;
  map_info.description = ".SEC map file folder.";
  map_info.otbm_version = 0;
  map_info.items_major_version = 0;
  map_info.items_minor_version = 0;
  map_info.created = "";

  dialog_.setSelectedMapInfo(map_info);

  // Attempt client auto-match for SRV clients only
  handleClientAutoMatch(folder);
}

void StartupController::handleNewMapFlow() {
  spdlog::info("Opening new map modal");
  
  // Just show the modal in StartupDialog - no state transition needed
  dialog_.showNewMapModal();
}

void StartupController::handleNewMapConfirmed(const UI::NewMapPanel::State& config) {
  spdlog::info("Creating new map: {} ({}x{})", config.map_name, 
               config.map_width, config.map_height);
  
  // Build NewMapConfig from the dialog state
  Services::NewMapConfig map_config;
  map_config.map_name = config.map_name;
  map_config.map_width = config.map_width;
  map_config.map_height = config.map_height;
  map_config.otbm_version = config.otbm_version;
  map_config.items_major = config.items_major;
  map_config.items_minor = config.items_minor;
  map_config.description = config.description;

  // Open save dialog to pick file location
  NFD::UniquePath outPath;
  nfdfilteritem_t filters[1] = {{"OTBM Maps", "otbm"}};
  std::string default_name = config.map_name + ".otbm";
  nfdresult_t result =
      NFD::SaveDialog(outPath, filters, 1, nullptr, default_name.c_str());

  if (result != NFD_OKAY) {
    return; // User cancelled save dialog
  }

  std::filesystem::path save_path = outPath.get();

  // Create and save the map to disk
  map_ops_.createAndSaveNewMap(map_config, save_path);

  // Auto-select the newly created map in the startup dialog
  handleMapSelection(save_path, 0);
}

void StartupController::handleLoadMap() {
  spdlog::info("Loading map: {}", selected_map_path_.string());

  map_ops_.handleOpenRecentMap(selected_map_path_, matched_client_index_);
}

void StartupController::handleClientConfiguration() {
  spdlog::info("Opening client configuration dialog");

  client_config_ctrl_ = std::make_unique<Presentation::ClientConfigurationController>();
  dialog_.getClientConfigDialog().open(*client_config_ctrl_, registry_, config_);
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
