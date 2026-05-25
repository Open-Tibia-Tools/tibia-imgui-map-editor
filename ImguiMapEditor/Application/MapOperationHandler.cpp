#include "MapOperationHandler.h"

#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include <nfd.hpp>
#include <spdlog/spdlog.h>

#include "IO/Otbm/OtbmReader.h"
#include "ImGuiNotify.hpp"
#include "MapConversionHandler.h"
#include "Services/ClientDataService.h"
#include "Services/Map/MapSavingService.h"
#include "Services/SpriteManager.h"
#include "UI/Map/MapPanel.h"
#include "Utils/ScopedFlag.h"

namespace MapEditor {
namespace AppLogic {

MapOperationHandler::MapOperationHandler(
    Services::ConfigService &config, Services::ClientVersionRegistry &versions,
    Services::RecentLocationsService &recent_locations,
    Services::ViewSettings &view_settings, AppLogic::MapTabManager &tab_manager,
    Brushes::BrushRegistry &brush_registry,
    Services::TilesetService &tileset_service)
    : config_(config), versions_(versions), recent_locations_(recent_locations),
      view_settings_(view_settings), tab_manager_(tab_manager),
      brush_registry_(brush_registry), tileset_service_(tileset_service) {

  loading_service_ = std::make_unique<Services::MapLoadingService>(
      versions, view_settings_, brush_registry_, tileset_service_);

  // Create conversion handler with notification callback
  // Map ConversionNotificationType to our NotificationType for consistency
  conversion_handler_ = std::make_unique<MapConversionHandler>(
      tab_manager,
      nullptr, // client_data set later via setExistingResources
      [this](ConversionNotificationType type, const std::string &msg) {
        // Convert ConversionNotificationType to NotificationType
        NotificationType nt;
        switch (type) {
        case ConversionNotificationType::Info:
          nt = NotificationType::Info;
          break;
        case ConversionNotificationType::Success:
          nt = NotificationType::Success;
          break;
        case ConversionNotificationType::Warning:
          nt = NotificationType::Warning;
          break;
        case ConversionNotificationType::Error:
          nt = NotificationType::Error;
          break;
        }
        notify(nt, msg);
      });
}

// Destructor must be defined in .cpp where MapConversionHandler is complete
MapOperationHandler::~MapOperationHandler() = default;

void MapOperationHandler::setExistingResources(
    Services::ClientDataService *client_data,
    Services::SpriteManager *sprite_manager) {
  existing_client_data_ = client_data;
  existing_sprite_manager_ = sprite_manager;

  // Update conversion handler with client data
  if (conversion_handler_) {
    conversion_handler_->setClientData(client_data);
  }
}

// handleNewMap() removed - NewMap is now handled via StartupDialog modal

void MapOperationHandler::handleOpenMap() {
  NFD::UniquePath outPath;
  nfdfilteritem_t filters[1] = {{"OTBM Maps", "otbm"}};
  nfdresult_t result = NFD::OpenDialog(outPath, filters, 1);

  if (result == NFD_OKAY) {
    pending_map_path_ = outPath.get();

    // Client data must already be loaded from StartupDialog
    if (existing_client_data_) {
      handleSecondMapOpen(pending_map_path_);
    } else {
      notify(NotificationType::Error,
             "No client loaded. Please restart and select a client.");
    }
  }
}

// handleOpenSecMap() removed - now handled by handleOpenSecMapDirect() via
// modal dialog

void MapOperationHandler::handleSaveMap() {
  auto *session = tab_manager_.getActiveSession();
  if (!session || !session->getMap()) {
    spdlog::warn("No active map to save");
    if (on_map_saved_)
      on_map_saved_(false);
    return;
  }

  auto *map = session->getMap();
  std::filesystem::path save_path = map->getFilename();

  if (save_path.empty()) {
    NFD::UniquePath outPath;
    nfdfilteritem_t filters[1] = {{"OTBM Maps", "otbm"}};
    nfdresult_t result =
        NFD::SaveDialog(outPath, filters, 1, nullptr, "untitled.otbm");

    if (result != NFD_OKAY) {
      if (on_map_saved_)
        on_map_saved_(false);
      return;
    }
    save_path = outPath.get();
    map->setFilename(save_path.string());
    session->setFilePath(save_path);
  }

  performSave(*session, save_path, false);
}

void MapOperationHandler::handleSaveAsMap() {
  auto *session = tab_manager_.getActiveSession();
  if (!session || !session->getMap()) {
    spdlog::warn("No active map to save");
    return;
  }

  auto *map = session->getMap();

  // Always show file dialog for Save As
  NFD::UniquePath outPath;
  nfdfilteritem_t filters[1] = {{"OTBM Maps", "otbm"}};

  // Suggest current filename if it exists
  std::string default_name = "untitled.otbm";
  std::filesystem::path current_path = map->getFilename();
  if (!current_path.empty()) {
    default_name = current_path.filename().string();
  }

  nfdresult_t result =
      NFD::SaveDialog(outPath, filters, 1, nullptr, default_name.c_str());

  if (result != NFD_OKAY) {
    return;
  }

  std::filesystem::path save_path = outPath.get();
  map->setFilename(save_path.string());
  session->setFilePath(save_path);

  performSave(*session, save_path, true);
}

void MapOperationHandler::performSave(EditorSession &session,
                                      const std::filesystem::path &save_path,
                                      bool is_save_as) {
  auto *map = session.getMap();
  if (!map)
    return;

  Utils::ScopedFlag loading(
      is_loading_); // Saving is also a "loading" operation for UI blocking

  spdlog::info("Saving map to: {}", save_path.string());

  Services::MapSavingService saving_service(existing_client_data_);
  saving_service.setSaveHouses(true);
  saving_service.setSaveSpawns(true);

  auto save_result = saving_service.save(
      save_path, *map, [](int percent, const std::string &status) {
        spdlog::info("Save progress: {}% - {}", percent, status);
      });

  if (save_result.success) {
    spdlog::info("Map saved successfully. {} tiles, {} items",
                 save_result.tiles_saved, save_result.items_saved);
    session.setModified(false);

    std::string success_message =
        is_save_as ? "Map saved as " + save_path.filename().string()
                   : "Map saved successfully!";
    notify(NotificationType::Success, success_message);

    if (on_map_saved_ && !is_save_as) {
      on_map_saved_(true);
    }
  } else {
    spdlog::error("Failed to save map: {}", save_result.error);
    notify(NotificationType::Error, "Failed to save map: " + save_result.error);
    if (on_map_saved_ && !is_save_as) {
      on_map_saved_(false);
    }
  }
}

void MapOperationHandler::handleSaveAllMaps() {
  for (auto i : std::views::iota(0, tab_manager_.getTabCount())) {
    auto *session = tab_manager_.getSession(i);
    if (session && session->isModified()) {
      tab_manager_.setActiveTab(i);
      handleSaveMap();
    }
  }
}

void MapOperationHandler::handleOpenRecentMap(const std::filesystem::path &path,
                                              uint32_t version) {
  pending_map_path_ = path;
  current_version_ = version;

  auto *client_version = versions_.getVersion(version);
  if (client_version && client_version->validateFiles()) {
    Utils::ScopedFlag loading(is_loading_);
    loadMapFromPath(path, version);
  } else {
    std::string reason;
    if (!client_version) {
      reason = "Client version " + std::to_string(version) + " not found.";
    } else if (client_version->getClientPath().empty()) {
      reason = "No client path configured for version " +
               std::to_string(version) + ".";
    } else if (!std::filesystem::exists(client_version->getClientPath())) {
      reason = "Client path does not exist:\n" +
               client_version->getClientPath().string();
    } else {
      auto missing = [](const std::filesystem::path &p) -> std::string {
        return std::filesystem::exists(p) ? "" : p.filename().string();
      };
      std::vector<std::string> missing_files;
      auto dat = missing(client_version->getDatPath());
      auto spr = missing(client_version->getSprPath());

      if (!dat.empty()) missing_files.push_back(dat);
      if (!spr.empty()) missing_files.push_back(spr);

      if (client_version->getDataSource() == ::MapEditor::Domain::ItemDataSource::OTB) {
          auto otb = missing(client_version->getItemMetadataPath());
          if (!otb.empty()) missing_files.push_back(otb);
      } else if (client_version->getDataSource() == ::MapEditor::Domain::ItemDataSource::SRV) {
          auto srv = missing(client_version->getItemMetadataPath());
          if (!srv.empty()) missing_files.push_back(srv);
      }
      for (size_t i = 0; i < missing_files.size(); ++i) {
        if (i > 0) reason += ", ";
        reason += missing_files[i];
      }
      reason += " missing from:\n" + client_version->getClientPath().string() +
                "\nPlease add missing files or configure the correct client path.";
    }
    notify(NotificationType::Error,
           "Client version " + std::to_string(version) +
               " not configured.\n" + reason);
  }
}

// handleProjectConfigConfirmed() removed - legacy ProjectConfigDialog flow no
// longer used

void MapOperationHandler::handleNewMapDirect(const std::string &map_name,
                                             uint16_t width, uint16_t height,
                                             uint32_t client_version) {
  spdlog::info("Creating new map directly: {} ({}x{}) version {}", map_name,
               width, height, client_version);

  current_version_ = client_version;
  pending_map_path_.clear();

  Utils::ScopedFlag loading(is_loading_);

  Services::NewMapConfig map_config;
  map_config.map_name = map_name;
  map_config.map_width = width;
  map_config.map_height = height;

  auto result = loading_service_->createNewMap(map_config, client_version);

  if (result.success) {
    transferNewResources(std::move(result));
  } else {
    notify(NotificationType::Error, "Failed to create new map");
  }
}

void MapOperationHandler::handleOpenSecMapDirect(
    const std::filesystem::path &sec_folder, uint32_t client_version) {
  spdlog::info("Opening SEC map directly: {} version {}", sec_folder.string(),
               client_version);

  current_version_ = client_version;
  pending_map_path_ = sec_folder;

  Utils::ScopedFlag loading(is_loading_);

  auto result = loading_service_->loadSecMap(sec_folder, client_version);

  if (result.success) {
    transferNewResources(std::move(result));
  } else {
    notify(NotificationType::Error, "Failed to load SEC map: " + result.error);
  }
}

void MapOperationHandler::loadMapFromPath(const std::filesystem::path &path,
                                          uint32_t version) {
  Services::MapLoadingResult result;

  if (existing_client_data_ && existing_sprite_manager_) {
    // Use existing client data - items will get correct ItemType pointers
    spdlog::info("[MapOperationHandler] Loading map with existing client data");
    result = loading_service_->loadMapWithExistingClientData(
        path, existing_client_data_, existing_sprite_manager_);
  } else {
    // First map load - create new client data
    spdlog::info("[MapOperationHandler] Loading map with new client data");
    result = loading_service_->loadMap(path, version, path);
  }

  if (result.success) {
    config_.addRecentFile(path.string());
    transferNewResources(std::move(result));
  }
}

void MapOperationHandler::transferNewResources(
    Services::MapLoadingResult result) {
  // Transfer resources from result to callback
  // When loading with existing client data, client_data and sprite_manager
  // are already null in the result - no branching needed
  if (on_map_loaded_) {
    on_map_loaded_(std::move(result.map), std::move(result.client_data),
                   std::move(result.sprite_manager), result.camera_center);
  }
}

void MapOperationHandler::notify(NotificationType type,
                                 const std::string &message) {
  if (on_notification_) {
    on_notification_(type, message);
  } else {
    // Fallback for tests or if callback not set: log to spdlog
    switch (type) {
    case NotificationType::Success:
      spdlog::info("Notify: {}", message);
      break;
    case NotificationType::Error:
      spdlog::error("Notify: {}", message);
      break;
    case NotificationType::Warning:
      spdlog::warn("Notify: {}", message);
      break;
    case NotificationType::Info:
      spdlog::info("Notify: {}", message);
      break;
    }
  }
}

void MapOperationHandler::handleConvertToServerId() {
  if (conversion_handler_) {
    conversion_handler_->convertToServerId();
  }
}

void MapOperationHandler::handleConvertToClientId() {
  if (conversion_handler_) {
    conversion_handler_->convertToClientId();
  }
}

void MapOperationHandler::handleSecondMapOpen(
    const std::filesystem::path &path) {
  spdlog::info("Opening second map: {}", path.string());

  // Read OTBM header to get map version info
  auto header_result = IO::OtbmReader::readHeader(path);
  if (!header_result.success) {
    notify(NotificationType::Error,
           "Failed to read map: " + header_result.error);
    return;
  }

  const auto &ver = header_result.version;
  uint32_t map_items_major = ver.client_version_major;
  uint32_t map_items_minor = ver.client_version_minor;

  spdlog::info("Map requires Items {}.{}", map_items_major, map_items_minor);

  // Check compatibility with current client
  auto compat = checkMapCompatibility(map_items_major, map_items_minor);

  if (compat.compatible) {
    // Compatible -> load directly using current version
    spdlog::info("Map is compatible, loading directly");
    config_.addRecentFile(path.string());
    Utils::ScopedFlag loading(is_loading_);
    loadMapFromPath(path, current_version_);
  } else {
    // Incompatible -> show compatibility popup
    spdlog::warn("Map is incompatible: {}", compat.error_message);
    compat.map_name = path.stem().string();
    spdlog::info("Showing compatibility popup...");
    compatibility_popup_.show(compat, path);
    spdlog::info("Compatibility popup show() returned, popup is_open: {}",
                 compatibility_popup_.isOpen());
  }
}

UI::MapCompatibilityResult
MapOperationHandler::checkMapCompatibility(uint32_t map_items_major,
                                           uint32_t map_items_minor) {
  UI::MapCompatibilityResult result;

  if (!existing_client_data_) {
    result.compatible = false;
    result.error_message = "No client data loaded";
    return result;
  }

  // Get current client version info
  auto *client_version = versions_.getVersion(current_version_);
  if (!client_version) {
    result.compatible = false;
    result.error_message = "Current client version not found";
    return result;
  }

  uint32_t client_items_major = client_version->getOtbMajor();
  uint32_t client_items_minor = client_version->getOtbVersion();

  result.map_items_major = map_items_major;
  result.map_items_minor = map_items_minor;
  result.client_items_major = client_items_major;
  result.client_items_minor = client_items_minor;
  result.client_version = current_version_;

  // Check compatibility - items major and minor must match
  bool major_match = (client_items_major == map_items_major);
  bool minor_match = (client_items_minor == map_items_minor);

  if (major_match && minor_match) {
    result.compatible = true;
  } else {
    result.compatible = false;
    result.error_message = "Items version mismatch: Map requires " +
                           std::to_string(map_items_major) + "." +
                           std::to_string(map_items_minor) +
                           " but client provides " +
                           std::to_string(client_items_major) + "." +
                           std::to_string(client_items_minor);
  }

  return result;
}

void MapOperationHandler::requestDeferredMapLoad(
    const std::filesystem::path &path, uint32_t version) {
  spdlog::info("Deferring map load to next frame: {}", path.string());
  deferred_load_pending_ = true;
  deferred_load_path_ = path;
  deferred_load_version_ = version;
}

void MapOperationHandler::processPendingMapLoad() {
  if (!deferred_load_pending_) {
    return;
  }

  spdlog::info("Processing deferred map load: {}",
               deferred_load_path_.string());
  deferred_load_pending_ = false;

  // Now safe to load - we're outside the render frame
  handleOpenRecentMap(deferred_load_path_, deferred_load_version_);
}

} // namespace AppLogic
} // namespace MapEditor
