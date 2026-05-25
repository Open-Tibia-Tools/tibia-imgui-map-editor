#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MapEditor {
namespace Domain {

constexpr uint32_t kOtbmVersionMin = 1;
constexpr uint32_t kOtbmVersionMax = 4;

enum class DatFormat : uint8_t {
  Unknown,
  Format74,
  Format755,
  Format78,
  Format86,
  Format96,
  Format1010,
  Format1050,
  Format1057
};

struct ClientAssetDetectionResult {
  std::optional<std::string> metadata_file_name;
  std::optional<std::string> sprites_file_name;
  std::optional<uint32_t> dat_signature;
  std::optional<uint32_t> spr_signature;
  std::optional<DatFormat> dat_format;
  std::optional<bool> transparency;
  std::optional<bool> extended;
  std::optional<bool> frame_durations;
  std::optional<bool> frame_groups;
  std::vector<std::string> warnings;
};

enum class PropertyVisualState {
  Default,
  Pending,
  Undetected,
  Saved,
};

enum class ItemDataSource : uint8_t {
  OTB,
  SRV,
  DAT
};

struct ClientTemplate {
  uint32_t version = 0;
  std::string name;
  std::string description;
  ItemDataSource data_source = ItemDataSource::OTB;
  uint32_t otb_id = 0;
  uint32_t otb_major = 0;
  std::vector<uint32_t> otbm_versions;
  uint32_t dat_signature = 0;
  uint32_t spr_signature = 0;
  bool transparency = false;
  bool extended = false;
  bool frame_durations = false;
  bool frame_groups = false;
  std::string data_directory;
};

} // namespace Domain
} // namespace MapEditor
