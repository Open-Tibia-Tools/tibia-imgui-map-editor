/**
 * @file BrushXmlReader.cpp
 * @brief Implementation of RME-format brush XML parsing.
 */

#include "BrushXmlReader.h"

#include "../Brushes/BrushRegistry.h"
#include "../Brushes/Data/BorderBlock.h"
#include "../Brushes/Data/DoodadAlternative.h"
#include "../Brushes/Data/WallNode.h"
#include "../Brushes/Enums/BrushEnums.h"
#include "../Services/Brushes/BorderLookupService.h"
#include "../Services/Brushes/CarpetLookupService.h"
#include "../Services/Brushes/TableLookupService.h"
#include "../Services/Brushes/WallLookupService.h"
#include "XmlUtils.h"
#include <spdlog/spdlog.h>


namespace MapEditor::IO {

namespace fs = std::filesystem;
using namespace Brushes;

BrushXmlReader::BrushXmlReader(Dependencies deps) : deps_(std::move(deps)) {}

bool BrushXmlReader::loadFile(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::warn("[BrushXmlReader] File not found: {}", path.string());
    return false;
  }

  std::string const absPath = fs::absolute(path).string();
  if (loadedFiles_.find(absPath) != loadedFiles_.end()) {
    spdlog::debug("[BrushXmlReader] Already loaded: {}", path.string());
    return true;
  }
  loadedFiles_.insert(absPath);

  pugi::xml_document doc;
  std::string error;

  // Try to load with <brushes> root
  pugi::xml_node root = XmlUtils::loadXmlFile(path, "brushes", doc, error);
  if (!root) {
    // Try with <materials> root (RME uses this)
    root = XmlUtils::loadXmlFile(path, "materials", doc, error);
    if (!root) {
      spdlog::error("[BrushXmlReader] {}", error);
      return false;
    }
  }

  lastLoadCount_ = 0;
  parseBrushesRoot(root, path);

  spdlog::info("[BrushXmlReader] Loaded {} brushes from {}", lastLoadCount_,
               path.filename().string());
  return true;
}

size_t BrushXmlReader::loadDirectory(const fs::path &dir) {
  if (!fs::is_directory(dir)) {
    spdlog::warn("[BrushXmlReader] Not a directory: {}", dir.string());
    return 0;
  }

  size_t totalLoaded = 0;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".xml") {
      if (loadFile(entry.path())) {
        totalLoaded += lastLoadCount_;
      }
    }
  }
  return totalLoaded;
}

void BrushXmlReader::parseBrushesRoot(const pugi::xml_node &root,
                                      const fs::path &sourceFile) {
  for (pugi::xml_node const child : root.children()) {
    std::string const nodeName = child.name();

    if (nodeName == "brush") {
      parseBrush(child);
    }
    // RME also supports ground, wall, doodad, table, carpet as direct children
    else if (nodeName == "ground" || nodeName == "wall" ||
             nodeName == "doodad" || nodeName == "table" ||
             nodeName == "carpet") {
      parseBrush(child);
    }
  }
}

void BrushXmlReader::parseBrush(const pugi::xml_node &node) {
  std::string name = node.attribute("name").as_string();
  if (name.empty()) {
    spdlog::warn("[BrushXmlReader] Skipping brush with empty name");
    return;
  }

  // Get brush type - either from 'type' attribute or from node name
  std::string typeStr = node.attribute("type").as_string();
  if (typeStr.empty()) {
    typeStr = node.name(); // Use node name as type (ground, wall, etc)
  }

  // Get look ID for preview
  uint32_t lookId = node.attribute("server_lookid").as_uint(0);
  if (lookId == 0) {
    lookId = node.attribute("lookid").as_uint(0);
  }

  // Parse by type
  if (typeStr == "ground") {
    parseGroundBrush(node, name, lookId);
  } else if (typeStr == "wall") {
    parseWallBrush(node, name, lookId);
  } else if (typeStr == "doodad") {
    parseDoodadBrush(node, name, lookId);
  } else if (typeStr == "table") {
    parseTableBrush(node, name, lookId);
  } else if (typeStr == "carpet") {
    parseCarpetBrush(node, name, lookId);
  } else {
    spdlog::debug("[BrushXmlReader] Unsupported brush type '{}' for '{}'",
                  typeStr, name);
    return;
  }

  ++lastLoadCount_;
}

void BrushXmlReader::parseGroundBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId) {
  // Get z-order for priority
  int zOrder = node.attribute("z-order").as_int(0);

  // Parse ground items
  std::vector<std::pair<uint32_t, uint32_t>> groundItems;
  for (pugi::xml_node const itemNode : node.children("item")) {
    uint32_t const id = itemNode.attribute("id").as_uint(0);
    uint32_t const chance = itemNode.attribute("chance").as_uint(1);
    if (id != 0) {
      groundItems.emplace_back(id, chance);
      if (lookId == 0) {
        lookId = id; // Use first item as look ID
      }
    }
  }

  // Parse borders
  BorderBlock borders;
  for (pugi::xml_node const borderNode : node.children("border")) {
    std::string const toName = borderNode.attribute("to").as_string();
    uint32_t const groundEquiv = borderNode.attribute("ground_equivalent").as_uint(0);
    borders.setGroundEquivalent(groundEquiv);

    // Parse border items
    for (pugi::xml_node const borderItem : borderNode.children("borderitem")) {
      std::string const edgeName = borderItem.attribute("edge").as_string();
      uint32_t const itemId = borderItem.attribute("id").as_uint(0);
      uint32_t const chance = borderItem.attribute("chance").as_uint(1);

      EdgeType const edge = parseEdgeName(edgeName);
      if (edge != EdgeType::None && itemId != 0) {
        borders.addItem(edge, itemId, chance);
      }
    }
  }

  // Parse friends/enemies
  std::vector<std::string> friends;
  std::vector<std::string> const enemies;
  for (pugi::xml_node const friendNode : node.children("friend")) {
    std::string const friendName = friendNode.attribute("name").as_string();
    if (!friendName.empty()) {
      friends.push_back(friendName);
    }
  }

  spdlog::debug(
      "[BrushXmlReader] Parsed ground brush '{}' with {} items, z-order {}",
      name, groundItems.size(), zOrder);

  // TODO: Create GroundBrush and register with BrushRegistry
  // This will be implemented in Phase 5 when GroundBrush class exists
}

void BrushXmlReader::parseWallBrush(const pugi::xml_node &node,
                                    const std::string &name, uint32_t lookId) {
  // Parse wall segments by type
  for (pugi::xml_node const wallNode : node.children("wall")) {
    std::string const typeStr = wallNode.attribute("type").as_string();
    WallAlign const align = parseWallType(typeStr);

    // Parse items for this wall type
    for (pugi::xml_node const itemNode : wallNode.children("item")) {
      uint32_t const id = itemNode.attribute("id").as_uint(0);
      uint32_t const chance = itemNode.attribute("chance").as_uint(1);
      if (id != 0 && lookId == 0) {
        lookId = id;
      }
    }
  }

  // Parse doors
  for (pugi::xml_node const doorNode : node.children("door")) {
    std::string const typeStr = doorNode.attribute("type").as_string();
    DoorType const doorType = parseDoorType(typeStr);

    for (pugi::xml_node const itemNode : doorNode.children("item")) {
      uint32_t const id = itemNode.attribute("id").as_uint(0);
      // Store door items by type
    }
  }

  spdlog::debug("[BrushXmlReader] Parsed wall brush '{}'", name);

  // TODO: Create WallBrush and register with BrushRegistry
  // This will be implemented in Phase 4 when WallBrush class exists
}

void BrushXmlReader::parseDoodadBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId) {
  bool const draggable = node.attribute("draggable").as_bool(true);
  bool const redoBorders = node.attribute("redo_borders").as_bool(false);
  bool const onBlocking = node.attribute("on_blocking").as_bool(false);
  bool const onDuplicate = node.attribute("on_duplicate").as_bool(false);

  // Parse alternatives
  std::vector<DoodadAlternative> alternatives;

  for (pugi::xml_node const altNode : node.children("alternate")) {
    DoodadAlternative alt;

    // Parse single items
    for (pugi::xml_node const itemNode : altNode.children("item")) {
      SingleItem item;
      item.itemId = itemNode.attribute("id").as_uint(0);
      item.chance = itemNode.attribute("chance").as_uint(1);
      if (item.itemId != 0) {
        alt.addSingleItem(item);
        if (lookId == 0) {
          lookId = item.itemId;
        }
      }
    }

    // Parse composites
    for (pugi::xml_node const compNode : altNode.children("composite")) {
      CompositeItem comp;
      comp.chance = compNode.attribute("chance").as_uint(1);

      for (pugi::xml_node const tileNode : compNode.children("tile")) {
        CompositeItem::TileOffset offset;
        offset.dx = tileNode.attribute("x").as_int(0);
        offset.dy = tileNode.attribute("y").as_int(0);
        offset.dz = tileNode.attribute("z").as_int(0);

        for (pugi::xml_node const itemNode : tileNode.children("item")) {
          SingleItem item;
          item.itemId = itemNode.attribute("id").as_uint(0);
          item.chance = itemNode.attribute("chance").as_uint(1);
          if (item.itemId != 0) {
            offset.items.push_back(item);
          }
        }

        comp.tiles.push_back(std::move(offset));
      }

      alt.addComposite(std::move(comp));
    }

    if (alt.hasContent()) {
      alternatives.push_back(std::move(alt));
    }
  }

  spdlog::debug(
      "[BrushXmlReader] Parsed doodad brush '{}' with {} alternatives", name,
      alternatives.size());

  // TODO: Create DoodadBrush and register with BrushRegistry
  // This will be implemented in Phase 2 when DoodadBrush class exists
}

void BrushXmlReader::parseTableBrush(const pugi::xml_node &node,
                                     const std::string &name, uint32_t lookId) {
  // Parse table items by alignment
  for (pugi::xml_node const tableNode : node.children("table")) {
    std::string const alignStr = tableNode.attribute("align").as_string();
    TableAlign const align = parseTableAlign(alignStr);

    for (pugi::xml_node const itemNode : tableNode.children("item")) {
      uint32_t const id = itemNode.attribute("id").as_uint(0);
      uint32_t const chance = itemNode.attribute("chance").as_uint(1);
      if (id != 0 && lookId == 0) {
        lookId = id;
      }
    }
  }

  spdlog::debug("[BrushXmlReader] Parsed table brush '{}'", name);

  // TODO: Create TableBrush and register with BrushRegistry
  // This will be implemented in Phase 3 when TableBrush class exists
}

void BrushXmlReader::parseCarpetBrush(const pugi::xml_node &node,
                                      const std::string &name,
                                      uint32_t lookId) {
  // Parse carpet items by edge type
  for (pugi::xml_node const carpetNode : node.children("carpet")) {
    std::string const alignStr = carpetNode.attribute("align").as_string();
    EdgeType const edge = parseEdgeName(alignStr);

    for (pugi::xml_node const itemNode : carpetNode.children("item")) {
      uint32_t const id = itemNode.attribute("id").as_uint(0);
      uint32_t const chance = itemNode.attribute("chance").as_uint(1);
      if (id != 0 && lookId == 0) {
        lookId = id;
      }
    }
  }

  spdlog::debug("[BrushXmlReader] Parsed carpet brush '{}'", name);

  // TODO: Create CarpetBrush and register with BrushRegistry
  // This will be implemented in Phase 3 when CarpetBrush class exists
}

} // namespace MapEditor::IO
