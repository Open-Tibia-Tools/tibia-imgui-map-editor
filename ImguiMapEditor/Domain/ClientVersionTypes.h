#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MapEditor {
namespace Domain {

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

} // namespace Domain
} // namespace MapEditor
