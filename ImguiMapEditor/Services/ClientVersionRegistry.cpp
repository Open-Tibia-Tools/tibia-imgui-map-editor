#include "ClientVersionRegistry.h"
#include "ClientVersionPersistence.h"
#include "ConfigService.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <vector>

namespace MapEditor {
namespace Services {

bool ClientVersionRegistry::loadDefaults(const ConfigService &config) {
  // Search for clients.json in standard locations
  std::vector<std::filesystem::path> search_paths = {
      "data/clients.json", "../data/clients.json", "clients.json"};

  for (const auto &path : search_paths) {
    if (std::filesystem::exists(path)) {
      // Use ClientVersionPersistence to load JSON
      auto data = ClientVersionPersistence::loadFromJson(path);
      if (!data.versions.empty()) {
        // Bulk load into registry
        loadVersions(path, std::move(data.versions),
                     std::move(data.otb_to_version), data.default_version);
        loadPathsFromConfig(config);
        spdlog::info("Loaded client versions from: {}", path.string());
        return true;
      }
    }
  }

  spdlog::warn("Could not find clients.json");
  return true;
}

void ClientVersionRegistry::loadVersions(
    const std::filesystem::path &json_path,
    std::map<uint32_t, Domain::ClientVersion> versions,
    std::map<uint32_t, uint32_t> otb_to_version, uint32_t default_version) {
  clients_json_path_ = json_path;
  versions_ = std::move(versions);
  otb_to_version_ = std::move(otb_to_version);
  default_version_ = default_version;

  spdlog::info("Loaded {} client versions", versions_.size());
}

void ClientVersionRegistry::loadPathsFromConfig(const ConfigService &config) {
  for (auto &[version_num, version] : versions_) {
    auto path = config.getClientPath(version_num);
    if (!path.empty()) {
      version.setClientPath(path);
    }
  }
}

void ClientVersionRegistry::savePathsToConfig(ConfigService &config) const {
  for (const auto &[version_num, version] : versions_) {
    auto path = version.getClientPath();
    if (!path.empty()) {
      config.setClientPath(version_num, path);
    }
  }
}

Domain::ClientVersion *
ClientVersionRegistry::getVersion(uint32_t version_number) {
  auto it = versions_.find(version_number);
  return it != versions_.end() ? &it->second : nullptr;
}

const Domain::ClientVersion *
ClientVersionRegistry::getVersion(uint32_t version_number) const {
  auto it = versions_.find(version_number);
  return it != versions_.end() ? &it->second : nullptr;
}

Domain::ClientVersion *
ClientVersionRegistry::getFirstByVersion(uint32_t version) {
  for (auto &[id, cv] : versions_)
    if (cv.getVersion() == version) return &cv;
  return nullptr;
}

const Domain::ClientVersion *
ClientVersionRegistry::getFirstByVersion(uint32_t version) const {
  for (const auto &[id, cv] : versions_)
    if (cv.getVersion() == version) return &cv;
  return nullptr;
}

Domain::ClientVersion *
ClientVersionRegistry::getVersionByOtbVersion(uint32_t otb_version) {
  auto it = otb_to_version_.find(otb_version);
  if (it != otb_to_version_.end()) {
    return getVersion(it->second);
  }
  return nullptr;
}

const Domain::ClientVersion *
ClientVersionRegistry::getVersionByOtbVersion(uint32_t otb_version) const {
  auto it = otb_to_version_.find(otb_version);
  if (it != otb_to_version_.end()) {
    return getVersion(it->second);
  }
  return nullptr;
}

std::vector<const Domain::ClientVersion *>
ClientVersionRegistry::getAllVersions() const {
  std::vector<const Domain::ClientVersion *> result;
  result.reserve(versions_.size());

  for (const auto &[num, version] : versions_) {
    result.push_back(&version);
  }

  // Sort by version number descending (newest first)
  std::sort(result.begin(), result.end(), [](const auto *a, const auto *b) {
    return a->getVersion() > b->getVersion();
  });

  return result;
}

std::vector<const Domain::ClientVersion *>
ClientVersionRegistry::getVisibleVersions() const {
  std::vector<const Domain::ClientVersion *> result;

  for (const auto &[num, version] : versions_) {
    if (version.isVisible()) {
      result.push_back(&version);
    }
  }

  // Sort by version number descending
  std::sort(result.begin(), result.end(), [](const auto *a, const auto *b) {
    return a->getVersion() > b->getVersion();
  });

  return result;
}

void ClientVersionRegistry::setClientPath(uint32_t version_number,
                                          const std::filesystem::path &path) {
  auto *version = getVersion(version_number);
  if (version) {
    version->setClientPath(path);
  }
}

Domain::ClientVersion *
ClientVersionRegistry::findVersionForOtb(uint32_t otb_minor_version) {
  // First try exact match
  auto *exact = getVersionByOtbVersion(otb_minor_version);
  if (exact) {
    return exact;
  }

  // Find closest version with lower or equal OTB version
  Domain::ClientVersion *best = nullptr;
  uint32_t best_otb = 0;

  for (auto &[num, version] : versions_) {
    uint32_t otb = version.getOtbVersion();
    if (otb <= otb_minor_version && otb > best_otb) {
      best = &version;
      best_otb = otb;
    }
  }

  return best;
}

bool ClientVersionRegistry::hasAnyValidPaths() const {
  for (const auto &[num, version] : versions_) {
    if (version.validateFiles()) {
      return true;
    }
  }
  return false;
}

void ClientVersionRegistry::setDefaultVersion(uint32_t version_number) {
  // Clear previous default
  for (auto &[num, version] : versions_) {
    version.setDefault(false);
  }

  // Set new default
  default_version_ = version_number;
  if (auto *version = getVersion(version_number)) {
    version->setDefault(true);
  }
}

bool ClientVersionRegistry::addClient(const Domain::ClientVersion &version) {
  uint32_t ver_num = version.getVersion();
  if (versions_.find(ver_num) != versions_.end()) {
    spdlog::warn("Cannot add client version {}: already exists", ver_num);
    return false;
  }

  versions_[ver_num] = version;

  // Add to OTB lookup if it has an OTB version
  if (version.getOtbVersion() > 0) {
    otb_to_version_[version.getOtbVersion()] = ver_num;
  }

  spdlog::info("Added client version {}", ver_num);
  return true;
}

bool ClientVersionRegistry::updateClient(uint32_t version_number,
                                         const Domain::ClientVersion &updated) {
  auto it = versions_.find(version_number);
  if (it == versions_.end()) {
    spdlog::warn("Cannot update client version {}: not found", version_number);
    return false;
  }

  // Remove old OTB mapping
  if (it->second.getOtbVersion() > 0) {
    otb_to_version_.erase(it->second.getOtbVersion());
  }

  // Update
  it->second = updated;

  // Add new OTB mapping
  if (updated.getOtbVersion() > 0) {
    otb_to_version_[updated.getOtbVersion()] = version_number;
  }

  spdlog::info("Updated client version {}", version_number);
  return true;
}

bool ClientVersionRegistry::removeClient(uint32_t version_number) {
  auto it = versions_.find(version_number);
  if (it == versions_.end()) {
    spdlog::warn("Cannot remove client version {}: not found", version_number);
    return false;
  }

  // Remove OTB mapping
  if (it->second.getOtbVersion() > 0) {
    otb_to_version_.erase(it->second.getOtbVersion());
  }

  // Clear default if removing default version
  if (default_version_ == version_number) {
    default_version_ = 0;
  }

  versions_.erase(it);
  spdlog::info("Removed client version {}", version_number);
  return true;
}

void ClientVersionRegistry::backupVersion(uint32_t version_number) {
  auto it = versions_.find(version_number);
  if (it != versions_.end()) {
    it->second.backup();
  }
}

void ClientVersionRegistry::restoreVersion(uint32_t version_number) {
  auto it = versions_.find(version_number);
  if (it != versions_.end()) {
    it->second.restore();
    it->second.clearDirty();
  }
}

} // namespace Services
} // namespace MapEditor
