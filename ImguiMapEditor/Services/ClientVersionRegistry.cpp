#include "ClientVersionRegistry.h"
#include "ClientVersionPersistence.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <vector>

namespace MapEditor {
namespace Services {

bool ClientVersionRegistry::loadDefaults(const ConfigService &config) {
  // 1. Load templates from client_templates.json
  std::vector<std::filesystem::path> template_paths = {
      "data/clients_templates.json", "../data/clients_templates.json", "clients_templates.json"};

  for (const auto &path : template_paths) {
    if (std::filesystem::exists(path)) {
      auto templates = ClientVersionPersistence::loadTemplatesFromJson(path);
      templates_ = std::move(templates);
      spdlog::info("Loaded {} client templates from: {}", templates_.size(), path.string());
      break;
    }
  }

  // 2. Load saved clients from clients_saved.json (if missing, empty list — no error)
  std::vector<std::filesystem::path> saved_paths = {
      "data/clients_saved.json", "../data/clients_saved.json", "clients_saved.json"};

  for (const auto &path : saved_paths) {
    if (std::filesystem::exists(path)) {
      auto data = ClientVersionPersistence::loadFromJson(path);
      versions_ = std::move(data.first);
      default_version_ = data.second;
      clients_json_path_ = path;
      if (!versions_.empty()) {
          setNextIndex(versions_.rbegin()->first);
      }
      spdlog::info("Loaded {} saved clients from: {}", versions_.size(), path.string());
      return true;
    }
  }

  // No saved clients yet — that's fine, user will add them
  spdlog::info("No saved clients found — starting with empty list");
  return true;
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

const Domain::ClientVersion *
ClientVersionRegistry::findBestMatch(uint32_t otb_minor, uint32_t items_major) const {
  const Domain::ClientVersion *best = nullptr;
  uint32_t best_index = UINT32_MAX;
  for (const auto &[index, cv] : versions_) {
    if (cv.getOtbVersion() == otb_minor && cv.getOtbMajor() == items_major) {
      if (index < best_index) {
        best = &cv;
        best_index = index;
      }
    }
  }
  return best;
}

const Domain::ClientVersion *
ClientVersionRegistry::findBestByVersion(uint32_t version) const {
  const Domain::ClientVersion *best = nullptr;
  uint32_t best_index = UINT32_MAX;
  for (const auto &[index, cv] : versions_) {
    if (cv.getVersion() == version) {
      if (index < best_index) {
        best = &cv;
        best_index = index;
      }
    }
  }
  return best;
}

std::vector<const Domain::ClientVersion *>
ClientVersionRegistry::getAllVersions() const {
  std::vector<const Domain::ClientVersion *> result;
  result.reserve(versions_.size());

  for (const auto &[idx, version] : versions_) {
    result.push_back(&version);
  }

  std::sort(result.begin(), result.end(), [](const auto *a, const auto *b) {
    return a->getIndex() < b->getIndex();
  });

  return result;
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
  for (auto &[num, version] : versions_) {
    version.setDefault(false);
  }

  default_version_ = version_number;
  if (auto *version = getVersion(version_number)) {
    version->setDefault(true);
  }
}

bool ClientVersionRegistry::addClient(const Domain::ClientVersion &version) {
  uint32_t idx = version.getIndex();
  if (versions_.find(idx) != versions_.end()) {
    spdlog::warn("Cannot add client index {}: already exists", idx);
    return false;
  }
  versions_[idx] = version;
  spdlog::info("Added client index {}", idx);
  return true;
}

bool ClientVersionRegistry::updateClient(uint32_t version_number,
                                         const Domain::ClientVersion &updated) {
  auto it = versions_.find(version_number);
  if (it == versions_.end()) {
    spdlog::warn("Cannot update client with index {}: not found", version_number);
    return false;
  }

  it->second = updated;
  it->second.setIndex(version_number);

  spdlog::info("Updated client with index {}", version_number);
  return true;
}

bool ClientVersionRegistry::removeClient(uint32_t version_number) {
  auto it = versions_.find(version_number);
  if (it == versions_.end()) {
    spdlog::warn("Cannot remove client with index {}: not found", version_number);
    return false;
  }

  if (default_version_ == version_number) {
    default_version_ = 0;
  }

  versions_.erase(it);
  spdlog::info("Removed client with index {}", version_number);
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
