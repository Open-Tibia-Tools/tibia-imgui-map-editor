#pragma once
#include "Domain/ChunkedMap.h"
#include "../NodeFileReader.h"
#include <filesystem>
#include <functional>
#include <string>

namespace MapEditor {

namespace Services {
class ClientDataService;
}

namespace IO {

class IMapBuilder;

/**
 * OTBM node types
 */
enum class OtbmNode : uint8_t {
  RootHeader = 0,
  MapData = 2,
  TileArea = 4,
  Tile = 5,
  Item = 6,
  Spawns = 9,
  SpawnArea = 10,
  Monster = 11,
  Towns = 12,
  Town = 13,
  HouseTile = 14,
  Waypoints = 15,
  Waypoint = 16
};

/**
 * OTBM attribute types
 */
enum class OtbmAttribute : uint8_t {
  Description = 1,
  ExtFile = 2,
  TileFlags = 3,
  ActionId = 4,
  UniqueId = 5,
  Text = 6,
  Desc = 7,
  TeleportDest = 8,
  Item = 9,
  DepotId = 10,
  ExtSpawnFile = 11,
  RuneCharges = 12,
  ExtHouseFile = 13,
  HouseDoorId = 14,
  Count = 15,
  Duration = 16,
  DecayingState = 17,
  WrittenDate = 18,
  WrittenBy = 19,
  SleeperGuid = 20,
  SleepStart = 21,
  Charges = 22,
  ExtSpawnNpcFile = 23,
  // IDs 24-39 reserved
  PodiumOutfit = 40,
  Tier = 41,
  AttributeMap = 128
};

/**
 * OTBM tile flags
 */
enum class OtbmTileFlag : uint32_t {
  None = 0,
  Protection = 1 << 0,
  Deprecated = 1 << 1,
  NoPvp = 1 << 2,
  NoLogout = 1 << 3,
  PvpZone = 1 << 4,
  Refresh = 1 << 5
};

/**
 * OTBM format versions
 */
enum class OtbmVersion : uint32_t { V1 = 0, V2 = 1, V3 = 2, V4 = 3 };

/**
 * Version info from OTBM file header
 */
struct OtbmVersionInfo {
  uint32_t otbm_version = 0;
  uint32_t client_version_major = 0;
  uint32_t client_version_minor = 0;
  uint32_t client_version = 0;

  uint16_t width = 0;
  uint16_t height = 0;
  std::string description;
  std::string spawn_file;
  std::string house_file;
};

/**
 * Result of OTBM parsing
 */
struct OtbmResult {
  bool success = false;
  std::string error;

  OtbmVersionInfo version;
  std::string spawn_file;
  std::string house_file;

  size_t tile_count = 0;
  size_t item_count = 0;
  size_t town_count = 0;
  size_t waypoint_count = 0;
};

/**
 * Extended result of OTBM parsing with map ownership.
 * Transfers ownership immediately - caller receives the map directly.
 */
struct OtbmReadResult {
  std::unique_ptr<Domain::ChunkedMap> map; // Ownership transferred to caller
  bool success = false;
  std::string error;

  OtbmVersionInfo version;
  std::string spawn_file;
  std::string house_file;

  size_t tile_count = 0;
  size_t item_count = 0;
  size_t town_count = 0;
  size_t waypoint_count = 0;
};

/**
 * Progress callback
 */
using OtbmProgressCallback =
    std::function<void(int percent, const std::string &status)>;

/**
 * OTBM map file reader.
 * Uses polymorphic IMapBuilder instead of templates for better separation.
 */
class OtbmReader {
public:
  /**
   * Read complete OTBM file and return map with ownership.
   * New preferred API - returns ownership immediately in result.
   */
  static OtbmReadResult read(const std::filesystem::path &path,
                             Services::ClientDataService *client_data = nullptr,
                             OtbmProgressCallback progress = nullptr);

  /**
   * Read only the header for version detection
   */
  static OtbmResult readHeader(const std::filesystem::path &path);

private:
  OtbmReader() = delete;

  // Internal implementation using builder
  static OtbmResult readInternal(const std::filesystem::path &path,
                                 IMapBuilder &builder,
                                 Services::ClientDataService *client_data,
                                 OtbmProgressCallback progress);

  static bool parseRootNode(BinaryNode *root, IMapBuilder &builder,
                            OtbmResult &result);

  static bool parseMapData(BinaryNode *mapDataNode, IMapBuilder &builder,
                           OtbmResult &result, DiskNodeFileReadHandle &file,
                           Services::ClientDataService *client_data,
                           OtbmProgressCallback progress);
};

} // namespace IO
} // namespace MapEditor
