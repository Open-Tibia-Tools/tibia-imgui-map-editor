#pragma once

#include "Domain/ClientVersionTypes.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace MapEditor {
namespace Domain {

/**
 * Represents a supported Tibia client version
 * Contains version info, paths to client data files, feature flags, and dirty tracking
 */
class ClientVersion {
public:
  ClientVersion() = default;
  ClientVersion(uint32_t version, const std::string &name,
                uint32_t otb_version);

  // Version identifiers
  uint32_t getVersion() const { return version_; }
  void setVersion(uint32_t v) { version_ = v; }
  const std::string &getName() const { return name_; }
  void setName(const std::string &v) { name_ = v; }
  uint32_t getOtbVersion() const { return otb_version_; }
  void setOtbVersion(uint32_t v) { otb_version_ = v; }
  uint32_t getOtbMajor() const { return otb_major_; }     // Items major version

  // OTBM versions supported (replaces single otbm_version_)
  const std::vector<uint32_t> &getMapVersionsSupported() const {
    return map_versions_supported_;
  }
  void setMapVersionsSupported(const std::vector<uint32_t> &vers) {
    map_versions_supported_ = vers;
  }
  uint32_t getOtbmVersion() const {
    return map_versions_supported_.empty() ? 0 : map_versions_supported_[0];
  }

  void setOtbMajor(uint32_t major) { otb_major_ = major; }
  void setOtbmVersion(uint32_t ver) {
    if (ver > 0) {
      map_versions_supported_ = {ver};
    }
  }

  // File signatures (for validation)
  uint32_t getDatSignature() const { return dat_signature_; }
  uint32_t getSprSignature() const { return spr_signature_; }
  void setDatSignature(uint32_t sig) { dat_signature_ = sig; }
  void setSprSignature(uint32_t sig) { spr_signature_ = sig; }

  // Client data path (user-configured)
  const std::filesystem::path &getClientPath() const { return client_path_; }
  void setClientPath(const std::filesystem::path &path) { client_path_ = path; }

  // Configurable file names
  const std::string &getMetadataFile() const { return metadata_file_; }
  void setMetadataFile(const std::string &file) { metadata_file_ = file; }
  const std::string &getSpritesFile() const { return sprites_file_; }
  void setSpritesFile(const std::string &file) { sprites_file_ = file; }

  ItemDataSource getDataSource() const { return data_source_; }
  void setDataSource(ItemDataSource source) { data_source_ = source; }

  // DatFormat (auto-derived from version)
  DatFormat getDatFormat() const { return dat_format_; }
  void setDatFormat(DatFormat fmt) { dat_format_ = fmt; }

  // Custom items database path (overrides auto-detection)
  const std::filesystem::path &getCustomItemsDbPath() const {
    return custom_items_db_path_;
  }
  void setCustomItemsDbPath(const std::filesystem::path &path) {
    custom_items_db_path_ = path;
  }

  // Feature flags
  bool isTransparent() const { return transparency_; }
  void setTransparent(bool v) { transparency_ = v; }
  bool isExtended() const { return extended_; }
  void setExtended(bool v) { extended_ = v; }
  bool hasFrameDurations() const { return frame_durations_; }
  void setFrameDurations(bool v) { frame_durations_ = v; }
  bool hasFrameGroups() const { return frame_groups_; }
  void setFrameGroups(bool v) { frame_groups_ = v; }

  // Feature detection based on version (backward compat)
  bool supportsExtendedSprites() const { return extended_ || version_ >= 960; }
  bool supportsFrameDurations() const {
    return frame_durations_ || version_ >= 1050;
  }
  bool supportsFrameGroups() const { return frame_groups_ || version_ >= 1057; }

  // Path helpers (use configurable metadata/sprites file names)
  std::filesystem::path getDatPath() const;
  std::filesystem::path getSprPath() const;
  std::filesystem::path getItemMetadataPath() const;

  // Validation
  bool hasValidPaths() const;
  bool validateFiles() const;

  // Visibility (some versions are internal/deprecated)
  bool isVisible() const { return visible_; }
  void setVisible(bool visible) { visible_ = visible; }

  // Default client flag
  bool isDefault() const { return is_default_; }
  void setDefault(bool is_default) { is_default_ = is_default; }

  // Data directory and description
  const std::string &getDataDirectory() const { return data_directory_; }
  void setDataDirectory(const std::string &dir) { data_directory_ = dir; }
  const std::string &getDescription() const { return description_; }
  void setDescription(const std::string &desc) { description_ = desc; }

  // Dirty tracking (backup/restore for undo in editor)
  bool isDirty() const { return is_dirty_; }
  void markDirty() { is_dirty_ = true; }
  void clearDirty() { is_dirty_ = false; }
  void backup();
  void restore();

private:
  uint32_t version_ = 0;
  std::string name_;
  uint32_t otb_version_ = 0;
  uint32_t otb_major_ = 0;
  std::vector<uint32_t> map_versions_supported_;
  uint32_t dat_signature_ = 0;
  uint32_t spr_signature_ = 0;
  std::filesystem::path client_path_;
  std::string data_directory_;
  std::string description_;
  std::string metadata_file_ = "Tibia.dat";
  std::string sprites_file_ = "Tibia.spr";
  ItemDataSource data_source_ = ItemDataSource::OTB;
  bool visible_ = true;
  bool is_default_ = false;
  DatFormat dat_format_ = DatFormat::Unknown;
  bool transparency_ = false;
  bool extended_ = false;
  bool frame_durations_ = false;
  bool frame_groups_ = false;
  bool is_dirty_ = false;
  std::filesystem::path custom_items_db_path_;

  struct BackupData {
    uint32_t version;
    std::string name;
    uint32_t otb_version;
    uint32_t otb_major;
    std::vector<uint32_t> map_versions_supported;
    uint32_t dat_signature;
    uint32_t spr_signature;
    std::filesystem::path client_path;
    std::string data_directory;
    std::string description;
    std::string metadata_file;
    std::string sprites_file;
    ItemDataSource data_source;
    bool visible;
    bool is_default;
    bool transparency;
    bool extended;
    bool frame_durations;
    bool frame_groups;
    DatFormat dat_format;
    std::filesystem::path custom_items_db_path;
  } backup_data_;
};

} // namespace Domain
} // namespace MapEditor
