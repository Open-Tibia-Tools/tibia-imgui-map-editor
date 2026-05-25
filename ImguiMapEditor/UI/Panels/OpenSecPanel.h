#pragma once
#include "Services/ClientVersionRegistry.h"
#include "Services/ClientVersionValidator.h"
#include "IO/SecReader.h"
#include <filesystem>
#include <string>
#include <cstdint>
#include "Core/Config.h"

namespace MapEditor {
namespace UI {

/**
 * Panel for Open SEC Map workflow.
 * SEC maps are directory-based (*.sec files) and require items.srv.
 * 
 * Single responsibility: SEC map folder selection and validation.
 */
class OpenSecPanel {
public:
    struct State {
        std::filesystem::path sec_folder;
        std::filesystem::path client_path;
        uint32_t selected_client_index = 0;
        bool has_items_srv = false;
        bool paths_valid = false;
        std::string validation_error;
        
        // Detected from scanning .sec files
        size_t sector_count = 0;
        int sector_x_min = 0, sector_x_max = 0;
        int sector_y_min = 0, sector_y_max = 0;
        int sector_z_min = 0, sector_z_max = 0;
        bool scan_valid = false;
    };
    
    OpenSecPanel();
    
    void initialize(Services::ClientVersionRegistry* registry,
                    Services::ClientVersionValidator* validator);
    
    struct RenderResult {
        bool state_changed = false;
        bool confirmed = false;
    };
    
    RenderResult render(State& state);
    
    void browseForSecFolder(State& state);
    void browseForClientFolder(State& state);

private:
    void renderSecFolderSelector(State& state);
    void renderClientSelector(State& state);
    void renderMapInfo(const State& state);
    void scanSecFolder(State& state);
    void validateClientForSec(State& state);
    
    Services::ClientVersionRegistry* registry_ = nullptr;
    Services::ClientVersionValidator* validator_ = nullptr;
    
    std::string sec_path_buffer_;
    std::string client_path_buffer_;
};

} // namespace UI
} // namespace MapEditor
