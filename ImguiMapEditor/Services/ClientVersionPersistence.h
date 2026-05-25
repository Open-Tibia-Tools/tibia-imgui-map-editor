#pragma once
#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include <filesystem>
#include <map>
#include <utility>
#include <vector>

namespace MapEditor {
namespace Services {

class ClientVersionPersistence {
public:
  static std::pair<std::map<uint32_t, Domain::ClientVersion>, uint32_t>
  loadFromJson(const std::filesystem::path &path);
  static bool saveToJson(const std::filesystem::path &path,
                         const std::map<uint32_t, Domain::ClientVersion> &versions,
                         uint32_t default_index);

  static std::vector<Domain::ClientTemplate> loadTemplatesFromJson(
      const std::filesystem::path &path);
};

} // namespace Services
} // namespace MapEditor
