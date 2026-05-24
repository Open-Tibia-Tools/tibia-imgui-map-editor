#pragma once
#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include <filesystem>
#include <map>
#include <vector>

namespace MapEditor {
namespace Services {

struct ClientVersionsData {
  std::map<uint32_t, Domain::ClientVersion> versions;
  std::map<uint32_t, uint32_t> otb_to_version;
  uint32_t default_version = 0;
};

class ClientVersionPersistence {
public:
  static ClientVersionsData loadFromJson(const std::filesystem::path &path);
  static bool saveToJson(const std::filesystem::path &path,
                         const ClientVersionsData &data);

  static std::vector<Domain::ClientTemplate> loadTemplatesFromJson(
      const std::filesystem::path &path);
};

} // namespace Services
} // namespace MapEditor
