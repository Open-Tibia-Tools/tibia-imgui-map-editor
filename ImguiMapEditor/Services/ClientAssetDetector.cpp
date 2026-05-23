#include "ClientAssetDetector.h"
#include "Core/Config.h"
#include "IO/Readers/DatReaderBase.h"
#include "IO/Readers/DatReaderFactory.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <spdlog/spdlog.h>
#include <vector>

namespace MapEditor {
namespace Services {

namespace {

constexpr size_t kSpritePixels = Config::Rendering::SPRITE_SIZE * Config::Rendering::SPRITE_SIZE;
constexpr size_t kMaxSampleOffsets = 24;

std::optional<uint32_t> readU32FromFile(const std::filesystem::path &path,
                                        std::vector<std::string> &warnings) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    warnings.push_back("Could not open file: " + path.string());
    return std::nullopt;
  }

  uint32_t value = 0;
  file.read(reinterpret_cast<char *>(&value), 4);
  if (!file) {
    warnings.push_back("Could not read header from: " + path.string());
    return std::nullopt;
  }

  return value;
}

bool readSpriteCount(std::ifstream &file, bool extended, uint32_t &sprite_count) {
  if (extended) {
    file.read(reinterpret_cast<char *>(&sprite_count), 4);
    return file.good();
  }

  uint16_t compact = 0;
  file.read(reinterpret_cast<char *>(&compact), 2);
  if (!file)
    return false;
  sprite_count = compact;
  return true;
}

bool validateSpriteRleEncoding(std::span<const uint8_t> blob, uint8_t color_bpp) {
  size_t read = 0;
  size_t written_pixels = 0;

  while (read + 4 <= blob.size() && written_pixels < kSpritePixels) {
    uint16_t transparent = static_cast<uint16_t>(blob[read] | (blob[read + 1] << 8));
    read += 2;
    uint16_t colored = static_cast<uint16_t>(blob[read] | (blob[read + 1] << 8));
    read += 2;

    if (written_pixels + transparent + colored > kSpritePixels) {
      return false;
    }

    const size_t colored_bytes = static_cast<size_t>(colored) * color_bpp;
    if (read + colored_bytes > blob.size()) {
      return false;
    }

    read += colored_bytes;
    written_pixels += transparent + colored;
  }

  return written_pixels == kSpritePixels && read == blob.size();
}

std::optional<std::vector<uint32_t>>
collectSpriteSamples(std::ifstream &file, bool extended, uint32_t &sprite_count) {
  if (!readSpriteCount(file, extended, sprite_count)) {
    return std::nullopt;
  }
  if (sprite_count == 0 || sprite_count > 3000000) {
    return std::nullopt;
  }

  const size_t header_size = 4 + (extended ? sizeof(uint32_t) : sizeof(uint16_t)) +
                             static_cast<size_t>(sprite_count) * sizeof(uint32_t);

  file.seekg(0, std::ios::end);
  const auto file_size = static_cast<size_t>(file.tellg());
  if (header_size > file_size) {
    return std::nullopt;
  }

  file.seekg(4 + (extended ? sizeof(uint32_t) : sizeof(uint16_t)), std::ios::beg);

  std::vector<uint32_t> sample_offsets;
  sample_offsets.reserve(kMaxSampleOffsets);

  uint32_t prev_nonzero = 0;
  for (uint32_t id = 1; id <= sprite_count; ++id) {
    uint32_t offset = 0;
    file.read(reinterpret_cast<char *>(&offset), 4);
    if (!file)
      return std::nullopt;
    if (offset == 0)
      continue;
    if (offset < header_size || offset > file_size - 5)
      return std::nullopt;
    if (prev_nonzero != 0 && offset < prev_nonzero)
      return std::nullopt;
    prev_nonzero = offset;
    if (sample_offsets.size() < kMaxSampleOffsets) {
      sample_offsets.push_back(offset);
    }
  }

  return sample_offsets;
}

struct SpriteProbeResult {
  bool extended = false;
  uint32_t sprite_count = 0;
  int opaque_matches = 0;
  int alpha_matches = 0;
};

std::optional<SpriteProbeResult> probeSpriteStructure(const std::filesystem::path &spr_path,
                                                      bool extended) {
  std::ifstream file(spr_path, std::ios::binary);
  if (!file)
    return std::nullopt;

  uint32_t signature = 0;
  file.read(reinterpret_cast<char *>(&signature), 4);
  if (!file)
    return std::nullopt;

  uint32_t sprite_count = 0;
  auto offsets = collectSpriteSamples(file, extended, sprite_count);
  if (!offsets)
    return std::nullopt;

  SpriteProbeResult result;
  result.extended = extended;
  result.sprite_count = sprite_count;

  for (uint32_t off : *offsets) {
    file.seekg(off + 3, std::ios::beg); // skip RGB transparent color
    if (!file)
      return std::nullopt;

    uint16_t compressed_size = 0;
    file.read(reinterpret_cast<char *>(&compressed_size), 2);
    if (!file)
      return std::nullopt;

    if (static_cast<size_t>(file.tellg()) + compressed_size >
        static_cast<size_t>(file.seekg(0, std::ios::end).tellg())) {
      return std::nullopt;
    }
    file.seekg(off + 3 + 2, std::ios::beg);

    std::vector<uint8_t> blob(compressed_size);
    file.read(reinterpret_cast<char *>(blob.data()), compressed_size);
    if (!file)
      return std::nullopt;

    if (validateSpriteRleEncoding(blob, 3)) {
      result.opaque_matches += 1;
    }
    if (validateSpriteRleEncoding(blob, 4)) {
      result.alpha_matches += 1;
    }
  }

  return result;
}

} // namespace

Domain::ClientAssetDetectionResult
ClientAssetDetector::detect(const std::filesystem::path &client_path,
                            const std::string &metadata_file,
                            const std::string &sprites_file,
                            const std::map<uint32_t, Domain::ClientVersion> *versions) {
  Domain::ClientAssetDetectionResult result;

  if (!std::filesystem::exists(client_path)) {
    result.warnings.push_back("Client path does not exist: " + client_path.string());
    return result;
  }

  auto dat_path = client_path / (metadata_file.empty() ? "Tibia.dat" : metadata_file);
  auto spr_path = client_path / (sprites_file.empty() ? "Tibia.spr" : sprites_file);

  if (std::filesystem::exists(dat_path)) {
    result.metadata_file_name = std::filesystem::path(dat_path).filename().string();
    result.dat_signature = readU32FromFile(dat_path, result.warnings);

    // DAT probing: try parsing with known version readers
    if (versions && result.dat_signature) {
      for (const auto &[ver_num, cv] : *versions) {
        if (cv.getDatSignature() != *result.dat_signature)
          continue;
        try {
          auto reader = IO::DatReaderFactory::create(ver_num);
          auto dat_result = reader->read(dat_path, *result.dat_signature);
          if (dat_result.success) {
            result.extended = reader->usesExtendedSprites();
            result.frame_durations = reader->hasFrameDurations();
            result.frame_groups = reader->hasFrameGroups();
            spdlog::info("DAT probe matched version {}: extended={} frameDurations={} frameGroups={}",
                         ver_num, *result.extended, *result.frame_durations, *result.frame_groups);
            break;
          }
        } catch (...) {
          continue;
        }
      }
      if (!result.extended)
        result.warnings.push_back("DAT probing could not determine features.");
    }
  } else {
    result.warnings.push_back("DAT file not found: " + dat_path.string());
  }

  if (std::filesystem::exists(spr_path)) {
    result.sprites_file_name = std::filesystem::path(spr_path).filename().string();
    result.spr_signature = readU32FromFile(spr_path, result.warnings);

    std::vector<SpriteProbeResult> candidates;
    for (bool ext : {false, true}) {
      if (auto c = probeSpriteStructure(spr_path, ext)) {
        candidates.push_back(*c);
      }
    }

    if (candidates.empty()) {
      result.warnings.push_back("Could not derive SPR structure from binary.");
    } else {
      auto best = std::max_element(
          candidates.begin(), candidates.end(),
          [](const SpriteProbeResult &a, const SpriteProbeResult &b) {
            return (a.opaque_matches + a.alpha_matches) <
                   (b.opaque_matches + b.alpha_matches);
          });

      if (best->opaque_matches > 0 || best->alpha_matches > 0) {
        // Cross-validate with DAT result
        if (result.extended && *result.extended != best->extended) {
          result.warnings.push_back("DAT and SPR disagree on extended flag. DAT takes precedence.");
          result.extended = *result.extended;
        } else {
          result.extended = best->extended;
        }
      }

      if (best->alpha_matches > best->opaque_matches) {
        result.transparency = true;
      } else if (best->opaque_matches > best->alpha_matches) {
        result.transparency = false;
      }
    }
  } else {
    result.warnings.push_back("SPR file not found: " + spr_path.string());
  }

  return result;
}

uint32_t
ClientAssetDetector::detectVersion(const std::filesystem::path &folder,
                                   const std::map<uint32_t, Domain::ClientVersion> &versions) {
  std::vector<std::string> warnings;
  auto dat_sig = readU32FromFile(folder / "Tibia.dat", warnings);
  auto spr_sig = readU32FromFile(folder / "Tibia.spr", warnings);

  if (!dat_sig || !spr_sig)
    return 0;

  for (const auto &[num, version] : versions) {
    if (version.getDatSignature() == *dat_sig && version.getSprSignature() == *spr_sig) {
      spdlog::info("Auto-detected client version {} from signatures", num);
      return num;
    }
  }

  for (const auto &[num, version] : versions) {
    if (version.getDatSignature() == *dat_sig) {
      spdlog::info("Auto-detected client version {} from DAT signature only", num);
      return num;
    }
  }

  spdlog::warn("No matching version for DAT: 0x{:08X}, SPR: 0x{:08X}", *dat_sig, *spr_sig);
  return 0;
}

} // namespace Services
} // namespace MapEditor
