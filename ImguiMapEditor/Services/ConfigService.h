#pragma once

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>
#include "Core/Config.h"

namespace MapEditor {
namespace Services {

/**
 * Service for application configuration persistence
 * Uses JSON format for human-readable storage
 */
class ConfigService {
public:
    ConfigService();
    ~ConfigService();
    
    // Load/save configuration
    bool load();
    bool save();
    
    // File path management
    void setConfigPath(const std::filesystem::path& path);
    const std::filesystem::path& getConfigPath() const { return config_path_; }
    
    // Generic value access
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        try {
            if (config_.contains(key)) {
                return config_[key].get<T>();
            }
        } catch (...) {}
        return default_value;
    }
    
    template<typename T>
    void set(const std::string& key, const T& value) {
        config_[key] = value;
        dirty_ = true;
    }
    
    bool contains(const std::string& key) const;
    void remove(const std::string& key);
    
    // Common settings shortcuts
    std::vector<std::string> getRecentFiles() const;
    void addRecentFile(const std::string& path);
    void clearRecentFiles();
    
    bool getShowWelcomeDialog() const;
    void setShowWelcomeDialog(bool show);
    
    // Window state
    int getWindowWidth() const;
    int getWindowHeight() const;
    bool getWindowMaximized() const;
    void setWindowState(int width, int height, bool maximized);
    
    // ImGui settings path (same directory as config.json)
    const std::string& getImGuiIniPath();

private:
    std::filesystem::path config_path_;
    nlohmann::json config_;
    bool dirty_ = false;
    std::string imgui_ini_path_;  // Stable string for ImGui IniFilename pointer
    
    static constexpr size_t MAX_RECENT_FILES = Config::Data::MAX_RECENT_FILES;
};

} // namespace Services
} // namespace MapEditor

