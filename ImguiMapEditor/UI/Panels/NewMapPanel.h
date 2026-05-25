#pragma once
#include "Services/ClientVersionRegistry.h"
#include "Services/RecentLocationsService.h"
#include <filesystem>
#include <string>
#include <cstdint>

namespace MapEditor {
namespace UI {

/**
 * Panel for New Map dialog.
 * Shows map name, dimensions, and recent clients.
 * 
 * Single responsibility: New Map mode UI rendering.
 */
class NewMapPanel {
public:
    struct State {
        std::string map_name = "Untitled";
        uint16_t map_width = 1024;
        uint16_t map_height = 1024;
        std::filesystem::path client_path;
        uint32_t selected_client_index = 0;
        bool paths_valid = false;
        std::string validation_error;
    };
    
    NewMapPanel();
    
    /**
     * Initialize panel with required services.
     */
    void initialize(Services::ClientVersionRegistry* registry,
                    Services::RecentLocationsService* recent);
    
    struct RenderResult {
        bool state_changed = false;
        bool confirmed = false;
    };

    /**
     * Render the New Map panel.
     * Returns RenderResult indicating state changes and confirmation.
     */
    RenderResult render(State& state);

private:
    void renderRecentClients(State& state);
    void renderClientPathSelector(State& state);
    void renderMapSettings(State& state);
    
    Services::ClientVersionRegistry* registry_ = nullptr;
    Services::RecentLocationsService* recent_ = nullptr;
    
    std::string name_buffer_;
    std::string path_buffer_;
};

} // namespace UI
} // namespace MapEditor
