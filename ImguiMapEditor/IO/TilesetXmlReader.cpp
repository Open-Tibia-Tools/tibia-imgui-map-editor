#include "TilesetXmlReader.h"

#include "../Brushes/BrushRegistry.h"
#include "../Brushes/Types/CreatureBrush.h"
#include "../Brushes/Types/PlaceholderBrush.h"
#include "../Brushes/Core/IBrush.h"
#include "../Domain/Outfit.h"
#include "../Domain/Tileset/TilesetRegistry.h"
#include "XmlUtils.h"
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>

namespace MapEditor::IO {

namespace fs = std::filesystem;
using namespace Domain::Tileset;
using namespace Brushes;

namespace {

bool isCollectionSourceFile(const fs::path &sourceFile) {
  std::string filename = sourceFile.filename().string();
  std::transform(filename.begin(), filename.end(), filename.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return filename.find("collection") != std::string::npos;
}

void markBrushMetadata(IBrush *brush, bool collectionTileset) {
  if (!brush) {
    return;
  }

  brush->flagAsVisible();
  if (collectionTileset) {
    brush->setCollection();
  }
}

} // namespace

TilesetXmlReader::TilesetXmlReader(
    Brushes::BrushRegistry &brushRegistry,
    Domain::Tileset::TilesetRegistry &tilesetRegistry)
    : brush_registry_(brushRegistry), tileset_registry_(tilesetRegistry) {}

TilesetXmlReader::~TilesetXmlReader() = default;

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

  const auto absoluteSourceFile = fs::absolute(sourceFile);
  Tileset *tileset =
      tileset_registry_.getTilesetBySourceFile(absoluteSourceFile);

  if (tileset) {
    // Tileset already exists for this exact source file.
    spdlog::debug("[TilesetXmlReader] Updating existing tileset: {}", name);
  } else {
    // Create new tileset
    auto newTileset = std::make_unique<Tileset>(name);
    newTileset->setSourceFile(absoluteSourceFile);
    tileset = newTileset.get();
    tileset_registry_.registerTileset(std::move(newTileset));
    spdlog::debug("[TilesetXmlReader] Created new tileset: {}", name);
  }

  const bool collectionTileset = isCollectionSourceFile(sourceFile);
  parseEntries(node, *tileset, collectionTileset);
  resolvePlaceholders(collectionTileset);

  spdlog::info("[TilesetXmlReader] Loaded tileset '{}' with {} entries from {}",
               name, tileset->size(), sourceFile.filename().string());
}

void TilesetXmlReader::parseEntries(const pugi::xml_node &node,
                                    Tileset &tileset,
                                    bool collectionTileset) {
  for (pugi::xml_node child : node.children()) {
    std::string childName = child.name();

    if (childName == "brush") {
      // Handle named brush reference
      std::string brushName = child.attribute("name").as_string();
      if (!brushName.empty()) {
        IBrush *brush = brush_registry_.getBrush(brushName);
        if (brush) {
          markBrushMetadata(brush, collectionTileset);
          tileset.addBrush(brush);
          if (brush->getType() == BrushType::Placeholder) {
            recordPlaceholder(brushName);
          }
        } else {
          IBrush *ptr = brush_registry_.getOrCreatePlaceholderBrush(brushName);
          markBrushMetadata(ptr, collectionTileset);
          recordPlaceholder(brushName);
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
            markBrushMetadata(brush, collectionTileset);
            tileset.addBrush(brush);
          }
        }
      } else if (id != 0) {
        // Single item
        IBrush *brush =
            brush_registry_.getOrCreateRAWBrush(static_cast<uint16_t>(id));
        if (brush) {
          markBrushMetadata(brush, collectionTileset);
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
        markBrushMetadata(brush, collectionTileset);
        tileset.addBrush(brush);
        if (brush->getType() == BrushType::Placeholder) {
          recordPlaceholder(creatureName);
        }
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
          markBrushMetadata(ptr, collectionTileset);
          tileset.addBrush(ptr);
          spdlog::debug("[TilesetXmlReader] Created creature brush: {}",
                        creatureName);
        } else {
          // Create placeholder - will be resolved later when creatures are
          // loaded
          IBrush *ptr = brush_registry_.getOrCreatePlaceholderBrush(creatureName);
          markBrushMetadata(ptr, collectionTileset);
          recordPlaceholder(creatureName);
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

void TilesetXmlReader::recordPlaceholder(std::string name) {
  if (name.empty()) {
    return;
  }

  ++placeholder_usage_[std::move(name)];
}

void TilesetXmlReader::decrementPlaceholderUsage(const std::string &name) {
  if (name.empty()) {
    return;
  }

  const auto it = placeholder_usage_.find(name);
  if (it == placeholder_usage_.end()) {
    return;
  }

  if (it->second <= 1) {
    placeholder_usage_.erase(it);
    return;
  }

  --it->second;
}

void TilesetXmlReader::resolvePlaceholders(bool collectionTileset) {
  if (placeholder_usage_.empty()) {
    return;
  }

  for (const auto &tilesetPtr : tileset_registry_.getAllTilesets()) {
    if (!tilesetPtr) {
      continue;
    }

    auto &entries = tilesetPtr->getEntriesMutable();
    for (auto &entry : entries) {
      if (!isBrush(entry)) {
        continue;
      }

      const auto *brush = getBrush(entry);
      if (!brush) {
        continue;
      }

      if (brush->getType() != BrushType::Placeholder) {
        continue;
      }

      const auto &placeholderName = brush->getName();
      auto *resolvedBrush = brush_registry_.getBrush(placeholderName);
      if (!resolvedBrush || resolvedBrush == brush ||
          resolvedBrush->getType() == BrushType::Placeholder) {
        continue;
      }

      resolvedBrush->flagAsVisible();
      if (collectionTileset || brush->hasCollection()) {
        resolvedBrush->setCollection();
      }
      entry = resolvedBrush;
      decrementPlaceholderUsage(placeholderName);
    }
  }
}

} // namespace MapEditor::IO
