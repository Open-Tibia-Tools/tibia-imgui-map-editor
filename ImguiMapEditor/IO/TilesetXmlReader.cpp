#include "TilesetXmlReader.h"

#include "../Brushes/BrushRegistry.h"
#include "../Brushes/Types/CreatureBrush.h"
#include "../Brushes/Types/PlaceholderBrush.h"
#include "../Domain/Outfit.h"
#include "../Domain/Tileset/TilesetRegistry.h"
#include "XmlUtils.h"
#include <spdlog/spdlog.h>

namespace MapEditor::IO {

namespace fs = std::filesystem;
using namespace Domain::Tileset;
using namespace Brushes;

TilesetXmlReader::TilesetXmlReader(
    Brushes::BrushRegistry &brushRegistry,
    Domain::Tileset::TilesetRegistry &tilesetRegistry)
    : brush_registry_(brushRegistry), tileset_registry_(tilesetRegistry) {}

bool TilesetXmlReader::loadTilesetFile(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::warn("[TilesetXmlReader] File not found: {}", path.string());
    return false;
  }

  std::string absPath = fs::absolute(path).string();
  if (loaded_files_.find(absPath) != loaded_files_.end()) {
    spdlog::debug("[TilesetXmlReader] Already loaded: {}", path.string());
    return true;
  }
  loaded_files_.insert(absPath);

  pugi::xml_document doc;
  std::string error;

  // Try to load as tileset root
  pugi::xml_node root = XmlUtils::loadXmlFile(path, "tileset", doc, error);
  if (!root) {
    spdlog::error("[TilesetXmlReader] {}", error);
    return false;
  }

  parseTilesetNode(root, path);
  return true;
}

void TilesetXmlReader::parseTilesetNode(const pugi::xml_node &node,
                                        const fs::path &sourceFile) {
  std::string name = node.attribute("name").as_string();
  if (name.empty()) {
    spdlog::warn("[TilesetXmlReader] Skipping tileset with empty name in {}",
                 sourceFile.string());
    return;
  }

  // Use injected registry instead of singleton
  Tileset *tileset = tileset_registry_.getTileset(name);

  if (tileset) {
    // Tileset already loaded — skip re-parsing to prevent entry duplication
    if (tileset->getSourceFile().empty()) {
      tileset->setSourceFile(fs::absolute(sourceFile));
    }
    spdlog::debug("[TilesetXmlReader] Tileset '{}' already loaded, skipping", name);
    return;
  } else {
    // Create new tileset
    auto newTileset = std::make_unique<Tileset>(name);
    newTileset->setSourceFile(fs::absolute(sourceFile));
    tileset = newTileset.get();
    tileset_registry_.registerTileset(std::move(newTileset));
    spdlog::debug("[TilesetXmlReader] Created new tileset: {}", name);
  }

  parseEntries(node, *tileset);

  spdlog::info("[TilesetXmlReader] Loaded tileset '{}' with {} entries from {}",
               name, tileset->size(), sourceFile.filename().string());
}

void TilesetXmlReader::parseEntries(const pugi::xml_node &node,
                                    Tileset &tileset) {
  for (pugi::xml_node child : node.children()) {
    std::string childName = child.name();

    if (childName == "brush") {
      // Handle named brush reference
      std::string brushName = child.attribute("name").as_string();
      if (!brushName.empty()) {
        IBrush *brush = brush_registry_.getBrush(brushName);
        if (brush) {
          tileset.addBrush(brush);
        } else {
          // Create placeholder for missing brush
          auto placeholder = std::make_unique<PlaceholderBrush>(brushName);
          IBrush *ptr = placeholder.get();
          brush_registry_.addBrush(std::move(placeholder));
          tileset.addBrush(ptr);
          spdlog::debug("[TilesetXmlReader] Created placeholder brush: {}",
                        brushName);
        }
      }
    } else if (childName == "item") {
      // Handle item by ID
      uint32_t fromId = child.attribute("fromid").as_uint(0);
      uint32_t toId = child.attribute("toid").as_uint(0);
      uint32_t id = child.attribute("id").as_uint(0);

      // Handle fromid without toid
      if (fromId != 0 && toId == 0) {
        toId = fromId;
      }

      if (fromId != 0 && toId != 0) {
        // Item range
        for (uint32_t i = fromId; i <= toId; ++i) {
          IBrush *brush =
              brush_registry_.getOrCreateRAWBrush(static_cast<uint16_t>(i));
          if (brush) {
            tileset.addBrush(brush);
          }
        }
      } else if (id != 0) {
        // Single item
        IBrush *brush =
            brush_registry_.getOrCreateRAWBrush(static_cast<uint16_t>(id));
        if (brush) {
          tileset.addBrush(brush);
        }
      }
    } else if (childName == "creature") {
      // Handle creature - support both inline and reference
      std::string creatureName = child.attribute("name").as_string();
      if (creatureName.empty()) {
        continue;
      }

      // Check if creature brush already exists
      IBrush *brush = brush_registry_.getBrush(creatureName);
      if (brush) {
        tileset.addBrush(brush);
      } else {
        // Check if inline definition provided
        std::string type = child.attribute("type").as_string();
        uint32_t looktype = child.attribute("looktype").as_uint(0);

        if (!type.empty() || looktype != 0) {
          // Create creature brush from inline definition
          bool isNpc = (type == "npc");
          Domain::Outfit outfit;
          outfit.lookType = static_cast<uint16_t>(looktype);
          outfit.lookHead =
              static_cast<uint8_t>(child.attribute("lookhead").as_uint(0));
          outfit.lookBody =
              static_cast<uint8_t>(child.attribute("lookbody").as_uint(0));
          outfit.lookLegs =
              static_cast<uint8_t>(child.attribute("looklegs").as_uint(0));
          outfit.lookFeet =
              static_cast<uint8_t>(child.attribute("lookfeet").as_uint(0));

          auto creatureBrush =
              std::make_unique<CreatureBrush>(creatureName, outfit);

          IBrush *ptr = creatureBrush.get();
          brush_registry_.addBrush(std::move(creatureBrush));
          tileset.addBrush(ptr);
          spdlog::debug("[TilesetXmlReader] Created creature brush: {}",
                        creatureName);
        } else {
          // Create placeholder - will be resolved later when creatures are
          // loaded
          auto placeholder = std::make_unique<PlaceholderBrush>(creatureName);
          IBrush *ptr = placeholder.get();
          brush_registry_.addBrush(std::move(placeholder));
          tileset.addBrush(ptr);
          spdlog::debug("[TilesetXmlReader] Created placeholder creature: {}",
                        creatureName);
        }
      }
    } else if (childName == "separator") {
      // Handle separator
      std::string separatorName = child.attribute("name").as_string();
      tileset.addSeparator(separatorName);
      spdlog::debug("[TilesetXmlReader] Added separator: {}",
                    separatorName.empty() ? "(unnamed)" : separatorName);
    }
  }
}

} // namespace MapEditor::IO
