#include "OtbmReader.h"
#include <filesystem>
#include <system_error>
#include <spdlog/spdlog.h>
#include "Domain/Item.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Services/ClientDataService.h"
#include "IMapBuilder.h"
#include "ChunkedMapBuilder.h"
#include "OtbmItemParser.h"
#include "OtbmTileParser.h"
namespace MapEditor {
namespace IO {

OtbmReadResult OtbmReader::read(const std::filesystem::path &path,
                                Services::ClientDataService *client_data,
                                OtbmProgressCallback progress) {
  OtbmReadResult read_result;

  // Create map - ownership will be transferred to caller
  read_result.map = std::make_unique<Domain::ChunkedMap>();

  ChunkedMapBuilder builder(*read_result.map);
  OtbmResult result = readInternal(path, builder, client_data, progress);

  // Copy result fields
  read_result.success = result.success;
  read_result.error = result.error;
  read_result.version = result.version;
  read_result.spawn_file = builder.getSpawnFile();
  read_result.house_file = builder.getHouseFile();
  read_result.tile_count = result.tile_count;
  read_result.item_count = result.item_count;
  read_result.town_count = result.town_count;
  read_result.waypoint_count = result.waypoint_count;

  // If failed, release the map
  if (!read_result.success) {
    read_result.map.reset();
  }

  return read_result;
}

OtbmResult OtbmReader::readInternal(const std::filesystem::path &path,
                                    IMapBuilder &builder,
                                    Services::ClientDataService *client_data,
                                    OtbmProgressCallback progress) {
  OtbmResult result;

  if (progress)
    progress(0, "Opening OTBM file...");

  std::error_code ec;
  auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    result.error = "Failed to stat file: " + ec.message();
    return result;
  }
  if (file_size < 4) {
    result.error = "File too small to be a valid OTBM map";
    return result;
  }

  DiskNodeFileReadHandle file(path, {"OTBM", "\0\0\0\0"});
  if (!file.isOk()) {
    result.error = "Failed to open file: " + file.getErrorMessage();
    return result;
  }

  BinaryNode *root = file.getRootNode();
  if (!root) {
    result.error = "Failed to read root node";
    return result;
  }

  if (progress)
    progress(5, "Parsing header...");

  if (!parseRootNode(root, builder, result)) {
    return result;
  }

  if (progress)
    progress(10, "Loading map data...");

  BinaryNode *mapDataNode = root->getChild();
  if (!mapDataNode) {
    result.error = "No map data node found";
    return result;
  }

  if (!parseMapData(mapDataNode, builder, result, file, client_data,
                    progress)) {
    return result;
  }

  if (progress)
    progress(100, "Map loading complete");

  result.success = true;
  spdlog::info("OTBM loaded: {} tiles, {} items, {} towns, {} waypoints",
               result.tile_count, result.item_count, result.town_count,
               result.waypoint_count);

  return result;
}

OtbmResult OtbmReader::readHeader(const std::filesystem::path &path) {
  OtbmResult result;

  DiskNodeFileReadHandle file(path, {"OTBM", "\0\0\0\0"});
  if (!file.isOk()) {
    result.error = "Failed to open file";
    return result;
  }

  BinaryNode *root = file.getRootNode();
  if (!root) {
    result.error = "Failed to read root node";
    return result;
  }

  uint8_t type;
  if (!root->getU8(type)) {
    result.error = "Failed to read root node type byte";
    spdlog::error("OtbmReader: {}", result.error);
    return result;
  }

  if (type != static_cast<uint8_t>(OtbmNode::RootHeader)) {
    result.error =
        "Invalid root node type: " + std::to_string(type) + " (expected 0)";
    spdlog::error("OtbmReader: {}", result.error);
    return result;
  }

  uint32_t version;
  if (!root->getU32(version)) {
    result.error = "Failed to read OTBM version";
    spdlog::error("OtbmReader: {}", result.error);
    return result;
  }
  result.version.otbm_version = version;

  uint16_t width, height;
  if (!root->getU16(width) || !root->getU16(height)) {
    result.error = "Failed to read map dimensions";
    spdlog::error("OtbmReader: {}", result.error);
    return result;
  }
  result.version.width = width;
  result.version.height = height;

  uint32_t major, minor;
  if (!root->getU32(major) || !root->getU32(minor)) {
    result.error = "Failed to read client version";
    spdlog::error("OtbmReader: {}", result.error);
    return result;
  }

  result.version.client_version_major = major;
  result.version.client_version_minor = minor;
  result.version.client_version = minor;

  // Try to read map data node for description and external files
  BinaryNode *mapDataNode = root->getChild();
  if (mapDataNode) {
    uint8_t mapDataType;
    if (mapDataNode->getU8(mapDataType) &&
        mapDataType == static_cast<uint8_t>(OtbmNode::MapData)) {

      bool done = false;
      uint8_t attr;
      while (!done && mapDataNode->getU8(attr)) {
        switch (attr) {
        case static_cast<uint8_t>(OtbmAttribute::Description): {
          std::string desc;
          if (mapDataNode->getString(desc)) {
            result.version.description = desc;
          }
          break;
        }
        case static_cast<uint8_t>(OtbmAttribute::ExtSpawnFile): {
          std::string spawn;
          if (mapDataNode->getString(spawn)) {
            result.version.spawn_file = spawn;
            result.spawn_file = spawn;
          }
          break;
        }
        case static_cast<uint8_t>(OtbmAttribute::ExtHouseFile): {
          std::string house;
          if (mapDataNode->getString(house)) {
            result.version.house_file = house;
            result.house_file = house;
          }
          break;
        }
        case static_cast<uint8_t>(OtbmAttribute::ExtSpawnNpcFile): {
          std::string ignored;
          mapDataNode->getString(ignored);
          break;
        }
        default:
          done = true;
          break;
        }
      }
    }
  }

  spdlog::info("OtbmReader: Header read successfully. Version: {}, Size: "
               "{}x{}, Client: {}.{}",
               version, width, height, major, minor);

  result.success = true;
  return result;
}

bool OtbmReader::parseRootNode(BinaryNode *root, IMapBuilder &builder,
                               OtbmResult &result) {
  uint8_t type;
  if (!root->getU8(type)) {
    result.error = "Failed to read root node type";
    spdlog::error("OtbmReader: {}", result.error);
    return false;
  }

  if (type != static_cast<uint8_t>(OtbmNode::RootHeader)) {
    result.error = "Invalid root node type: " + std::to_string(type);
    spdlog::error("OtbmReader: {}", result.error);
    return false;
  }

  uint32_t version;
  if (!root->getU32(version)) {
    result.error = "Failed to read OTBM version";
    spdlog::error("OtbmReader: {}", result.error);
    return false;
  }
  result.version.otbm_version = version;

  spdlog::info("OtbmReader: Loading map version {}", version);

  if (version > static_cast<uint32_t>(OtbmVersion::V4)) {
    spdlog::warn("Unsupported OTBM version {}, attempting to load anyway",
                 version);
  }

  uint16_t width, height;
  if (!root->getU16(width) || !root->getU16(height)) {
    result.error = "Failed to read map dimensions";
    spdlog::error("OtbmReader: {}", result.error);
    return false;
  }

  uint32_t otb_major, otb_minor;
  if (!root->getU32(otb_major) || !root->getU32(otb_minor)) {
    result.error = "Failed to read OTB version";
    spdlog::error("OtbmReader: {}", result.error);
    return false;
  }

  result.version.client_version_major = otb_major;
  result.version.client_version_minor = otb_minor;
  result.version.client_version = otb_minor;

  builder.setSize(width, height);

  spdlog::info("OTBM v{}, size {}x{}, client version {}.{}", version, width,
               height, otb_major, otb_minor);

  return true;
}

bool OtbmReader::parseMapData(BinaryNode *mapDataNode, IMapBuilder &builder,
                              OtbmResult &result, DiskNodeFileReadHandle &file,
                              Services::ClientDataService *client_data,
                              OtbmProgressCallback progress) {
  uint8_t type;
  if (!mapDataNode->getU8(type)) {
    result.error = "Failed to read map data node type";
    return false;
  }

  if (type != static_cast<uint8_t>(OtbmNode::MapData)) {
    result.error = "Expected MapData node, got: " + std::to_string(type);
    return false;
  }

  // Read map data attributes
  bool done_attrs = false;
  uint8_t attr;
  while (!done_attrs && mapDataNode->getU8(attr)) {
    switch (attr) {
    case static_cast<uint8_t>(OtbmAttribute::Description): {
      std::string desc;
      if (mapDataNode->getString(desc)) {
        builder.setDescription(desc);
      }
      break;
    }
    case static_cast<uint8_t>(OtbmAttribute::ExtSpawnFile): {
      std::string spawn;
      if (mapDataNode->getString(spawn)) {
        spdlog::info("OtbmReader: Found spawn file: {}", spawn);
        builder.setSpawnFile(spawn);
        result.spawn_file = spawn;
      }
      break;
    }
    case static_cast<uint8_t>(OtbmAttribute::ExtHouseFile): {
      std::string house;
      if (mapDataNode->getString(house)) {
        spdlog::info("OtbmReader: Found house file: {}", house);
        builder.setHouseFile(house);
        result.house_file = house;
      }
      break;
    }
    case static_cast<uint8_t>(OtbmAttribute::ExtSpawnNpcFile): {
      std::string ignored;
      mapDataNode->getString(ignored);
      break;
    }
    default:
      spdlog::trace("Unknown map attribute 0x{:02X}", attr);
      done_attrs = true;
      break;
    }
  }

  // Process child nodes
  int nodes_processed = 0;
  size_t total_size = file.size();

  for (BinaryNode *child = mapDataNode->getChild(); child != nullptr;
       child = child->advance()) {

    ++nodes_processed;

    if (progress && nodes_processed % 15 == 0) {
      int percent = 10;
      if (total_size > 0) {
        percent += static_cast<int>(80.0 * file.tell() / total_size);
      }
      progress(std::min(90, percent), "Loading tiles...");
    }

    uint8_t node_type;
    if (!child->getU8(node_type)) {
      spdlog::warn("Invalid map child node");
      continue;
    }

    switch (node_type) {
    case static_cast<uint8_t>(OtbmNode::TileArea):
      if (!OtbmTileParser::parseTileArea(child, builder, result, client_data)) {
        spdlog::warn("Failed to parse tile area");
      }
      break;

    case static_cast<uint8_t>(OtbmNode::Towns):
      if (!OtbmTileParser::parseTowns(child, builder, result)) {
        spdlog::warn("Failed to parse towns");
      }
      break;

    case static_cast<uint8_t>(OtbmNode::Waypoints):
      if (!OtbmTileParser::parseWaypoints(child, builder, result)) {
        spdlog::warn("Failed to parse waypoints");
      }
      break;

    default:
      spdlog::debug("Unknown map data child type: {}", node_type);
      break;
    }
  }

  return true;
}

} // namespace IO
} // namespace MapEditor
