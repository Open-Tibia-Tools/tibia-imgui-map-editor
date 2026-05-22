#include "PaletteXmlReader.h"

#include "Domain/Palette/Palette.h"
#include "Domain/Tileset/TilesetRegistry.h"
#include "IO/XmlUtils.h"
#include <algorithm>
#include <pugixml.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor::IO {

namespace fs = std::filesystem;
using namespace Domain::Palette;
using namespace Domain::Tileset;

PaletteXmlReader::PaletteXmlReader(
    Domain::Tileset::TilesetRegistry &tilesetRegistry,
    Domain::Palette::PaletteRegistry &paletteRegistry)
    : tileset_registry_(tilesetRegistry), palette_registry_(paletteRegistry) {}

bool PaletteXmlReader::load(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::error("[PaletteXmlReader] File not found: {}", path.string());
    return false;
  }

  pugi::xml_document doc;
  std::string error;
  pugi::xml_node root = XmlUtils::loadXmlFile(path, "palettes", doc, error);

  if (!root) {
    spdlog::error("[PaletteXmlReader] {}", error);
    return false;
  }

  fs::path basePath = path.parent_path();
  loadedFiles_.clear();
  loadedFiles_.insert(fs::absolute(path).string());

  int paletteCount = 0;
  for (pugi::xml_node paletteNode : root.children("palette")) {
    parsePaletteNode(paletteNode, basePath, path);
    paletteCount++;
  }

  spdlog::info("[PaletteXmlReader] Loaded {} palettes from {}", paletteCount,
               path.string());
  return true;
}

void PaletteXmlReader::parsePaletteNode(const pugi::xml_node &node,
                                        const fs::path &basePath,
                                        const fs::path &sourceFile) {
  std::string name = node.attribute("name").as_string();
  if (name.empty()) {
    spdlog::warn("[PaletteXmlReader] Skipping palette with empty name");
    return;
  }

  // Check if palette already exists - use injected registry
  if (palette_registry_.getPalette(name)) {
    spdlog::warn("[PaletteXmlReader] Palette '{}' already exists, skipping",
                 name);
    return;
  }

  auto palette = std::make_unique<Palette>(name);
  palette->setSourceFile(sourceFile);

  // Process <tileset> children
  for (pugi::xml_node tilesetNode : node.children("tileset")) {
    auto tilesets = processTilesetIncludes(tilesetNode, basePath);

    for (auto *tileset : tilesets) {
      if (tileset) {
        palette->addTileset(tileset);
        spdlog::debug("[PaletteXmlReader] Added tileset '{}' to palette '{}'",
                      tileset->getName(), name);
      } else {
        spdlog::warn("[PaletteXmlReader] Encountered null tileset for palette '{}'",
                     name);
      }
    }
  }

  if (palette->isEmpty()) {
    spdlog::warn("[PaletteXmlReader] Palette '{}' has no tilesets", name);
  }

  spdlog::info("[PaletteXmlReader] Registered palette '{}' with {} tilesets",
               name, palette->getTilesetCount());
  palette_registry_.registerPalette(std::move(palette));
}

std::vector<Tileset *>
PaletteXmlReader::processTilesetIncludes(const pugi::xml_node &tilesetNode,
                                         const fs::path &basePath) {

  std::vector<Tileset *> tilesets;

  for (pugi::xml_node include : tilesetNode.children("include")) {
    // Check for file include
    std::string file = include.attribute("file").as_string();
    if (!file.empty()) {
      fs::path filePath = basePath / file;

      // Check if specific tileset attribute is set
      std::string specificTileset = include.attribute("tileset").as_string();
      if (!specificTileset.empty()) {
        if (auto *tileset = tileset_registry_.getTileset(specificTileset)) {
          tilesets.push_back(tileset);
        }
        continue;
      }

      if (auto *tileset =
              tileset_registry_.getTilesetBySourceFile(fs::absolute(filePath))) {
        tilesets.push_back(tileset);
        continue;
      }

      // Fallback for legacy callers that only registered by display name.
      std::string name = getTilesetNameFromFile(filePath);
      if (!name.empty()) {
        if (auto *tileset = tileset_registry_.getTileset(name)) {
          tilesets.push_back(tileset);
        }
      }
      continue;
    }

    // Check for folder include
    std::string folder = include.attribute("folder").as_string();
    if (!folder.empty()) {
      fs::path folderPath = basePath / folder;
      bool recursive = include.attribute("subfolders").as_bool(false);

      if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
        auto files = collectXmlFiles(folderPath, recursive);
        for (const auto &xmlFile : files) {
          if (auto *tileset =
                  tileset_registry_.getTilesetBySourceFile(fs::absolute(xmlFile))) {
            tilesets.push_back(tileset);
            continue;
          }

          std::string name = getTilesetNameFromFile(xmlFile);
          if (!name.empty()) {
            if (auto *tileset = tileset_registry_.getTileset(name)) {
              tilesets.push_back(tileset);
            }
          }
        }
      }
    }
  }

  return tilesets;
}

std::vector<fs::path> PaletteXmlReader::collectXmlFiles(const fs::path &folder,
                                                        bool recursive) {

  std::vector<fs::path> result;

  try {
    if (recursive) {
      for (const auto &entry : fs::recursive_directory_iterator(folder)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
          result.push_back(entry.path());
        }
      }
    } else {
      for (const auto &entry : fs::directory_iterator(folder)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
          result.push_back(entry.path());
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    spdlog::error("[PaletteXmlReader] Error scanning folder {}: {}",
                  folder.string(), e.what());
  }

  std::sort(result.begin(), result.end());
  return result;
}

std::string PaletteXmlReader::getTilesetNameFromFile(const fs::path &file) {
  if (!fs::exists(file)) {
    spdlog::warn("[PaletteXmlReader] File not found: {}", file.string());
    return "";
  }

  pugi::xml_document doc;
  if (!doc.load_file(file.string().c_str())) {
    spdlog::warn("[PaletteXmlReader] Failed to parse: {}", file.string());
    return "";
  }

  // Look for <tileset name="..."> root element
  pugi::xml_node tileset = doc.child("tileset");
  if (tileset) {
    return tileset.attribute("name").as_string();
  }

  return "";
}

} // namespace MapEditor::IO
