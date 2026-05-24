#include "ClientVersionPersistence.h"
#include <fstream>
#include <iomanip>
#include <limits>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

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

    uint32_t id = client.value("id", client["version"].get<uint32_t>());
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

    Domain::ClientVersion version(id, version_number, name, otb_id);
    version.setId(id);
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
      version.setDataSource(Domain::ItemDataSource::SRV);
    } else if (source_str == "DAT") {
      version.setDataSource(Domain::ItemDataSource::DAT);
    } else if (source_str == "OTB") {
      version.setDataSource(Domain::ItemDataSource::OTB);
    } else {
      spdlog::warn("Unknown itemDataSource '{}' for client {}, falling back to 'OTB'", source_str, version_number);
      version.setDataSource(Domain::ItemDataSource::OTB);
    }

    version.setTransparent(client.value("transparency", false));
    version.setExtended(client.value("extended", false));
    version.setFrameDurations(client.value("frameDurations", false));
    version.setFrameGroups(client.value("frameGroups", false));

    if (is_default) {
      result.default_version = id;
    }

    if (result.versions.find(id) == result.versions.end()) {
      result.versions[id] = version;
      if (otb_id > 0) {
        result.otb_to_version[otb_id] = version_number;
        spdlog::debug("Mapped otbId {} -> version {}", otb_id, version_number);
      }
    }
  }

  spdlog::info("Loaded {} client versions from {}",
                result.versions.size(), path.string());
  return result;
}

bool ClientVersionPersistence::saveToJson(const std::filesystem::path &path,
                                          const ClientVersionsData &data) {
  nlohmann::json root;
  root["$schema"] = "./clients.schema.json";
  root["clients"] = nlohmann::json::array();

  for (const auto &[id, client] : data.versions) {
    nlohmann::json entry;
    entry["id"] = id;
    entry["version"] = client.getVersion();
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
    case Domain::ItemDataSource::OTB:
      entry["itemDataSource"] = "OTB";
      break;
    case Domain::ItemDataSource::SRV:
      entry["itemDataSource"] = "SRV";
      break;
    case Domain::ItemDataSource::DAT:
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

std::vector<Domain::ClientTemplate> ClientVersionPersistence::loadTemplatesFromJson(
    const std::filesystem::path &path) {
  std::vector<Domain::ClientTemplate> templates;
  std::ifstream file(path);
  if (!file.is_open()) return templates;

  nlohmann::json json;
  try { file >> json; } catch (...) { return templates; }
  if (!json.contains("clients") || !json["clients"].is_array()) return templates;

  for (const auto &client : json["clients"]) {
    if (!client.contains("version")) continue;
    Domain::ClientTemplate tpl;
    tpl.version = client["version"].get<uint32_t>();
    tpl.name = client.value("name", "");
    tpl.description = client.value("description", "");
    tpl.otb_id = client.value("otbId", 0);
    tpl.otb_major = client.value("otbMajor", 0);

    std::string source = client.value("itemDataSource", "OTB");
    if (source == "SRV") tpl.data_source = Domain::ItemDataSource::SRV;
    else if (source == "DAT") tpl.data_source = Domain::ItemDataSource::DAT;

    if (client.contains("otbmVersions") && client["otbmVersions"].is_array())
      for (const auto &v : client["otbmVersions"]) tpl.otbm_versions.push_back(v.get<uint32_t>());

    std::string ds = client.value("datSignature", "0");
    std::string ss = client.value("sprSignature", "0");
    try { tpl.dat_signature = static_cast<uint32_t>(std::stoul(ds, nullptr, 16)); } catch (...) {}
    try { tpl.spr_signature = static_cast<uint32_t>(std::stoul(ss, nullptr, 16)); } catch (...) {}

    tpl.transparency = client.value("transparency", false);
    tpl.extended = client.value("extended", false);
    tpl.frame_durations = client.value("frameDurations", false);
    tpl.frame_groups = client.value("frameGroups", false);
    tpl.data_directory = client.value("dataDirectory", "");

    templates.push_back(std::move(tpl));
  }
  return templates;
}

} // namespace Services
} // namespace MapEditor
