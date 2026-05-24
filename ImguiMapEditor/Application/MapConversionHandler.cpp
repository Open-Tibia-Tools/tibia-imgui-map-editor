#include "MapConversionHandler.h"
#include "MapTabManager.h"
#include "Domain/ChunkedMap.h"
#include "Services/ClientDataService.h"
#include "IO/Otbm/OtbmIdConverter.h"
#include <nfd.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace AppLogic {

MapConversionHandler::MapConversionHandler(
    MapTabManager& tab_manager,
    Services::ClientDataService* client_data,
    ConversionNotifyCallback notify_callback)
    : tab_manager_(tab_manager)
    , client_data_(client_data)
    , notify_(std::move(notify_callback))
{
}

void MapConversionHandler::convertToServerId() {
    executeConversion(
        IO::ConversionDirection::ClientToServer,
        "_server",
        "Server ID");
}

void MapConversionHandler::convertToClientId() {
    executeConversion(
        IO::ConversionDirection::ServerToClient,
        "_client",
        "Client ID");
}

void MapConversionHandler::executeConversion(
    IO::ConversionDirection direction,
    const std::string& suffix,
    const std::string& label)
{
    auto* session = tab_manager_.getActiveSession();
    if (!session || !session->getMap()) {
        notify_(ConversionNotificationType::Warning, "No active map to convert");
        return;
    }
    
    if (!client_data_) {
        notify_(ConversionNotificationType::Error, "Client data not loaded");
        return;
    }
    
    auto* map = session->getMap();
    std::filesystem::path const input_path = map->getFilename();
    
    if (input_path.empty()) {
        notify_(ConversionNotificationType::Error, "Map must be saved before converting");
        return;
    }
    
    // Show Save As dialog
    NFD::UniquePath outPath;
    nfdfilteritem_t filters[1] = {{"OTBM Maps", "otbm"}};
    
    std::string const default_name = input_path.stem().string() + suffix + ".otbm";
    
    nfdresult_t const nfd_result = NFD::SaveDialog(outPath, filters, 1, nullptr, default_name.c_str());
    if (nfd_result != NFD_OKAY) {
        notify_(ConversionNotificationType::Info, "Conversion cancelled");
        return;
    }
    
    std::filesystem::path const output_path = outPath.get();
    
    // Binary-level conversion: read OTBM, swap IDs, write back
    // No domain objects, no complex writer - just byte-level ID swap
    auto result = IO::OtbmIdConverter::convert(
        input_path,
        output_path,
        direction,
        client_data_
    );
    
    if (!result.success) {
        notify_(ConversionNotificationType::Error, "Conversion failed: " + result.error);
        return;
    }
    
    notify_(ConversionNotificationType::Success, 
           "Converted to " + label + ": " + std::to_string(result.items_converted) + 
           " items converted, " + std::to_string(result.items_skipped) + " skipped. Saved to " + 
           output_path.filename().string());
}

} // namespace AppLogic
} // namespace MapEditor
