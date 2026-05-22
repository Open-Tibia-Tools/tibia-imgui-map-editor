#include "ConfigService.h"
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

namespace {

std::optional<std::string> getEnvironmentVariable(const char *name) {
  if (!name || name[0] == '\0') {
    return std::nullopt;
  }

#ifdef _WIN32
  char *value = nullptr;
  size_t length = 0;
  if (_dupenv_s(&value, &length, name) != 0 || !value) {
    return std::nullopt;
  }

  std::string result{value};
  std::free(value);
  return result;
#else
  if (const char *value = std::getenv(name); value) {
    return std::string{value};
  }
  return std::nullopt;
#endif
}

} // namespace

ConfigService::ConfigService() {
    // Default config path in user's home directory
    #ifdef _WIN32
    if (const auto appdata = getEnvironmentVariable("APPDATA"); appdata) {
        config_path_ = std::filesystem::path(*appdata) / "TibiaMapEditor" / "config.json";
    }
    #else
    if (const auto home = getEnvironmentVariable("HOME"); home) {
        config_path_ = std::filesystem::path(*home) / ".config" / "TibiaMapEditor" / "config.json";
    }
    #endif
}

ConfigService::~ConfigService() {
    if (dirty_) {
        save();
    }
}

bool ConfigService::load() {
    if (config_path_.empty()) {
        spdlog::warn("Config path not set");
        return false;
    }
    
    if (!std::filesystem::exists(config_path_)) {
        spdlog::info("Config file does not exist, using defaults: {}", config_path_.string());
        config_ = nlohmann::json::object();
        return true;
    }
    
    try {
        std::ifstream file(config_path_);
        if (!file) {
            spdlog::error("Failed to open config file: {}", config_path_.string());
            return false;
        }
        
        file >> config_;
        dirty_ = false;
        spdlog::info("Loaded config from: {}", config_path_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse config: {}", e.what());
        config_ = nlohmann::json::object();
        return false;
    }
}

bool ConfigService::save() {
    if (config_path_.empty()) {
        spdlog::warn("Config path not set, cannot save");
        return false;
    }
    
    try {
        // Ensure directory exists
        auto dir = config_path_.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
        
        std::ofstream file(config_path_);
        if (!file) {
            spdlog::error("Failed to create config file: {}", config_path_.string());
            return false;
        }
        
        file << config_.dump(2);  // Pretty print with 2-space indent
        dirty_ = false;
        spdlog::info("Saved config to: {}", config_path_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
        return false;
    }
}

void ConfigService::setConfigPath(const std::filesystem::path& path) {
    config_path_ = path;
}

bool ConfigService::contains(const std::string& key) const {
    return config_.contains(key);
}

void ConfigService::remove(const std::string& key) {
    config_.erase(key);
    dirty_ = true;
}

std::string ConfigService::getLastClientVersion() const {
    return get<std::string>("last_client_version", "");
}

void ConfigService::setLastClientVersion(const std::string& version) {
    set("last_client_version", version);
}

std::filesystem::path ConfigService::getClientPath(uint32_t version) const {
    try {
        if (config_.contains("client_paths")) {
            auto& paths = config_["client_paths"];
            std::string ver_str = std::to_string(version);
            if (paths.contains(ver_str)) {
                return std::filesystem::path(paths[ver_str].get<std::string>());
            }
        }
    } catch (...) {}
    return std::filesystem::path{};
}

void ConfigService::setClientPath(uint32_t version, const std::filesystem::path& path) {
    if (!config_.contains("client_paths")) {
        config_["client_paths"] = nlohmann::json::object();
    }
    config_["client_paths"][std::to_string(version)] = path.string();
    dirty_ = true;
}

std::vector<std::string> ConfigService::getRecentFiles() const {
    return get<std::vector<std::string>>("recent_files", {});
}

void ConfigService::addRecentFile(const std::string& path) {
    auto recent = getRecentFiles();
    
    // Remove if already exists
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    
    // Insert at front
    recent.insert(recent.begin(), path);
    
    // Limit size
    if (recent.size() > MAX_RECENT_FILES) {
        recent.resize(MAX_RECENT_FILES);
    }
    
    set("recent_files", recent);
}

void ConfigService::clearRecentFiles() {
    set("recent_files", std::vector<std::string>{});
}

bool ConfigService::getShowWelcomeDialog() const {
    return get<bool>("show_welcome_dialog", true);
}

void ConfigService::setShowWelcomeDialog(bool show) {
    set("show_welcome_dialog", show);
}

int ConfigService::getWindowWidth() const {
    return get<int>("window_width", 1280);
}

int ConfigService::getWindowHeight() const {
    return get<int>("window_height", 720);
}

bool ConfigService::getWindowMaximized() const {
    return get<bool>("window_maximized", false);
}

void ConfigService::setWindowState(int width, int height, bool maximized) {
    set("window_width", width);
    set("window_height", height);
    set("window_maximized", maximized);
}

const std::string& ConfigService::getImGuiIniPath() {
    if (imgui_ini_path_.empty() && !config_path_.empty()) {
        imgui_ini_path_ = (config_path_.parent_path() / "imgui.ini").string();
    }
    return imgui_ini_path_;
}

} // namespace Services
} // namespace MapEditor

