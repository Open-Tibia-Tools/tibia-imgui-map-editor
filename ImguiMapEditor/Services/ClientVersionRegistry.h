#pragma once
#include "Domain/ClientVersion.h"
#include <filesystem>
#include <map>
#include <memory>
#include <vector>

namespace MapEditor {
namespace Services {

class ConfigService;

class ClientVersionRegistry {
public:
  ClientVersionRegistry() = default;
  ~ClientVersionRegistry() = default;

  bool loadDefaults(const ConfigService &config);

  Domain::ClientVersion *getVersion(uint32_t version_number);
  const Domain::ClientVersion *getVersion(uint32_t version_number) const;

  const Domain::ClientVersion *findBestMatch(uint32_t otb_minor, uint32_t items_major) const;
  const Domain::ClientVersion *findBestByVersion(uint32_t version) const;

  std::vector<const Domain::ClientVersion *> getAllVersions() const;

  bool hasAnyValidPaths() const;

  void setDefaultVersion(uint32_t version_number);
  uint32_t getDefaultVersion() const { return default_version_; }

  bool addClient(const Domain::ClientVersion &version);
  bool updateClient(uint32_t version_number, const Domain::ClientVersion &updated);
  bool removeClient(uint32_t version_number);

  const std::filesystem::path &getJsonPath() const { return clients_json_path_; }
  const std::filesystem::path &getConfigPath() const { return config_json_path_; }
  void setConfigPath(const std::filesystem::path &p) { config_json_path_ = p; }

  uint32_t nextIndex() { return ++next_index_; }
  void setNextIndex(uint32_t index) { if (index >= next_index_) next_index_ = index; }

  void loadTemplates(const std::vector<Domain::ClientTemplate> &templates) { templates_ = templates; }
  const std::vector<Domain::ClientTemplate> &getTemplates() const { return templates_; }

  const std::map<uint32_t, Domain::ClientVersion> &getVersionsMap() const {
    return versions_;
  }

  size_t getVersionCount() const { return versions_.size(); }
  bool isEmpty() const { return versions_.empty(); }

  void backupVersion(uint32_t version_number);
  void restoreVersion(uint32_t version_number);

private:
  std::map<uint32_t, Domain::ClientVersion> versions_;
  uint32_t default_version_ = 0;
  uint32_t next_index_ = 0;
  std::filesystem::path clients_json_path_;
  std::filesystem::path config_json_path_;
  std::vector<Domain::ClientTemplate> templates_;
};

} // namespace Services
} // namespace MapEditor
