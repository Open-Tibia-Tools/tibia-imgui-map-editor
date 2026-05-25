#include "ClientVersionPersistence.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

ClientVersionsData
ClientVersionPersistence::loadFromJson(const std::filesystem::path &path) {
  ClientVersionsData result;

  if (!std::filesystem::exists(path)) {
    spdlog::error("Failed to load clients.json: file not found at {}",
                  path.string());
    return result;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    spdlog::error("Failed to open clients.json: {}", path.string());
    return result;
  }

  nlohmann::json json;
  try {
    json = nlohmann::json::parse(file);
  } catch (const nlohmann::json::parse_error &e) {
    spdlog::error("Failed to parse clients.json: {}", e.what());
    return result;
  }

  if (!json.contains("clients") || !json["clients"].is_array()) {
    spdlog::error("Invalid clients.json structure: missing 'clients' array");
    return result;
  }

  for (const auto &client : json["clients"]) {
    if (!client.contains("version") || !client.contains("name")) {
      continue;
    }

    uint32_t version_number = client["version"].get<uint32_t>();
    std::string name = client["name"].get<std::string>();
    std::string description = client.value("description", "Client " + name);

    uint32_t otb_id = client.value("otbId", 0u);
    uint32_t otb_major = client.value("otbMajor", 0u);
    bool is_default = client.value("default", false);

    // Parse hex signatures
    uint32_t dat_sig = 0;
    uint32_t spr_sig = 0;

    if (client.contains("datSignature")) {
      const std::string &dat_str = client["datSignature"].get<std::string>();
      if (!dat_str.empty()) {
        try {
          unsigned long parsed = std::stoul(dat_str, nullptr, 16);
          if (parsed > std::numeric_limits<uint32_t>::max()) {
            spdlog::warn("datSignature '{}' for client {} exceeds uint32_t range",
                         dat_str, version_number);
          } else {
            dat_sig = static_cast<uint32_t>(parsed);
          }
        } catch (const std::exception &e) {
          spdlog::warn("Invalid datSignature '{}' for client {}: {}", dat_str,
                       version_number, e.what());
        }
      }
    }
    if (client.contains("sprSignature")) {
      const std::string &spr_str = client["sprSignature"].get<std::string>();
      if (!spr_str.empty()) {
        try {
          unsigned long parsed = std::stoul(spr_str, nullptr, 16);
          if (parsed > std::numeric_limits<uint32_t>::max()) {
            spdlog::warn("sprSignature '{}' for client {} exceeds uint32_t range",
                         spr_str, version_number);
          } else {
            spr_sig = static_cast<uint32_t>(parsed);
          }
        } catch (const std::exception &e) {
          spdlog::warn("Invalid sprSignature '{}' for client {}: {}", spr_str,
                       version_number, e.what());
        }
      }
    }

    if (version_number == 0) {
      continue;
    }

    Domain::ClientVersion version(version_number, name, otb_id);
    version.setDatSignature(dat_sig);
    version.setSprSignature(spr_sig);
    version.setOtbMajor(otb_major);

    if (client.contains("otbmVersions") && client["otbmVersions"].is_array() &&
        !client["otbmVersions"].empty()) {
      std::vector<uint32_t> otbm_versions;
      for (const auto &ver : client["otbmVersions"]) {
        otbm_versions.push_back(ver.get<uint32_t>());
      }
      version.setMapVersionsSupported(otbm_versions);
    }

    std::string data_dir = client.value("dataDirectory", "");
    version.setDataDirectory(data_dir);
    version.setDescription(description);
    version.setVisible(true);
    version.setDefault(is_default);

    version.setMetadataFile(client.value("metadataFile", "Tibia.dat"));
    version.setSpritesFile(client.value("spritesFile", "Tibia.spr"));

    std::string source_str = client.value("itemDataSource", "OTB");
    if (source_str == "SRV") {
      version.setDataSource(::MapEditor::Domain::ItemDataSource::SRV);
    } else if (source_str == "DAT") {
      version.setDataSource(::MapEditor::Domain::ItemDataSource::DAT);
    } else if (source_str == "OTB") {
      version.setDataSource(::MapEditor::Domain::ItemDataSource::OTB);
    } else {
      spdlog::warn("Unknown itemDataSource '{}' for client {}, falling back to 'OTB'", source_str, version_number);
      version.setDataSource(::MapEditor::Domain::ItemDataSource::OTB);
    }

    version.setTransparent(client.value("transparency", false));
    version.setExtended(client.value("extended", false));
    version.setFrameDurations(client.value("frameDurations", false));
    version.setFrameGroups(client.value("frameGroups", false));

    if (is_default) {
      result.default_version = version_number;
    }

    if (result.versions.find(version_number) == result.versions.end()) {
      result.versions[version_number] = version;
      if (otb_id > 0) {
        result.otb_to_version[otb_id] = version_number;
        spdlog::debug("Mapped otbId {} -> version {}", otb_id, version_number);
      }
    }
  }

  spdlog::info("Loaded {} client versions from clients.json",
               result.versions.size());
  return result;
}

bool ClientVersionPersistence::saveToJson(const std::filesystem::path &path,
                                          const ClientVersionsData &data) {
  nlohmann::json root;
  root[""] = "./clients.schema.json";
  root["clients"] = nlohmann::json::array();

  for (const auto &[ver_num, client] : data.versions) {
    nlohmann::json entry;
    entry["version"] = ver_num;
    entry["name"] = client.getName();
    entry["description"] = client.getDescription();
    entry["otbId"] = client.getOtbVersion();
    entry["otbMajor"] = client.getOtbMajor();
    entry["dataDirectory"] = client.getDataDirectory();

    // Format signatures as hex strings
    std::ostringstream dat_ss, spr_ss;
    dat_ss << std::uppercase << std::hex << client.getDatSignature();
    spr_ss << std::uppercase << std::hex << client.getSprSignature();
    entry["datSignature"] = dat_ss.str();
    entry["sprSignature"] = spr_ss.str();

    const auto &otbm_versions = client.getMapVersionsSupported();
    if (!otbm_versions.empty()) {
      entry["otbmVersions"] = otbm_versions;
    }

    entry["metadataFile"] = client.getMetadataFile();
    entry["spritesFile"] = client.getSpritesFile();

    switch (client.getDataSource()) {
    case ::MapEditor::Domain::ItemDataSource::OTB:
      entry["itemDataSource"] = "OTB";
      break;
    case ::MapEditor::Domain::ItemDataSource::SRV:
      entry["itemDataSource"] = "SRV";
      break;
    case ::MapEditor::Domain::ItemDataSource::DAT:
      entry["itemDataSource"] = "DAT";
      break;
    default:
      entry["itemDataSource"] = "OTB";
      break;
    }

    entry["transparency"] = client.isTransparent();
    entry["extended"] = client.isExtended();
    entry["frameDurations"] = client.hasFrameDurations();
    entry["frameGroups"] = client.hasFrameGroups();

    if (client.isDefault()) {
      entry["default"] = true;
    }

    root["clients"].push_back(entry);
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    spdlog::error("Failed to open clients.json for writing: {}", path.string());
    return false;
  }

  file << root.dump(2);
  spdlog::info("Saved {} clients to {}", data.versions.size(), path.string());
  return true;
}

} // namespace Services
} // namespace MapEditor
