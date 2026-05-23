#pragma once

#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include <filesystem>
#include <map>

namespace MapEditor {
namespace Services {

class ClientAssetDetector {
public:
  static Domain::ClientAssetDetectionResult
  detect(const std::filesystem::path &client_path,
         const std::string &metadata_file,
         const std::string &sprites_file,
         const std::map<uint32_t, Domain::ClientVersion> *versions = nullptr);

  static uint32_t
  detectVersion(const std::filesystem::path &folder,
                const std::map<uint32_t, Domain::ClientVersion> &versions);

private:
  static std::optional<uint32_t> readSignature(const std::filesystem::path &path,
                                               std::vector<std::string> &warnings);
};

} // namespace Services
} // namespace MapEditor
