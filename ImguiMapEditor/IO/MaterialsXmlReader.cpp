#include "MaterialsXmlReader.h"

#include "IO/PaletteXmlReader.h"
#include "IO/TilesetXmlReader.h"
#include "IO/XmlUtils.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor::IO {

namespace fs = std::filesystem;

MaterialsXmlReader::MaterialsXmlReader(
    Brushes::BrushRegistry &brushRegistry,
    Domain::Tileset::TilesetRegistry &tilesetRegistry,
    Domain::Palette::PaletteRegistry &paletteRegistry)
    : brushRegistry_(brushRegistry), tilesetRegistry_(tilesetRegistry),
      paletteRegistry_(paletteRegistry) {}

bool MaterialsXmlReader::load(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::error("[MaterialsXmlReader] File not found: {}", path.string());
    return false;
  }

  pugi::xml_document doc;
  std::string error;
  pugi::xml_node const root = XmlUtils::loadXmlFile(path, "materials", doc, error);

  if (!root) {
    spdlog::error("[MaterialsXmlReader] {}", error);
    return false;
  }

  fs::path const basePath = path.parent_path();
  loadedFiles_.clear();
  loadedFiles_.insert(fs::absolute(path).string());

  spdlog::info("[MaterialsXmlReader] Loading materials from: {}",
               path.string());

  // Process sections in dependency order
  for (pugi::xml_node const child : root.children()) {
    std::string const nodeName = child.name();

    if (nodeName == "borders") {
      processBordersNode(child, basePath);
    } else if (nodeName == "brushes") {
      processBrushesNode(child, basePath);
    } else if (nodeName == "creatures") {
      processCreaturesNode(child, basePath);
    } else if (nodeName == "items") {
      processItemsNode(child, basePath);
    } else if (nodeName == "tilesets") {
      processTilesetsNode(child, basePath);
    } else if (nodeName == "palettes") {
      processPalettesNode(child, basePath);
    }
  }

  spdlog::info("[MaterialsXmlReader] Materials loading complete");
  return true;
}

void MaterialsXmlReader::processBordersNode(const pugi::xml_node &node,
                                            const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing borders section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    // TODO: Implement BorderXmlReader when needed
    spdlog::debug("[MaterialsXmlReader] Would load border file: {}",
                  file.string());
  });
}

void MaterialsXmlReader::processBrushesNode(const pugi::xml_node &node,
                                            const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing brushes section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    // TODO: Implement BrushXmlReader when needed
    spdlog::debug("[MaterialsXmlReader] Would load brush file: {}",
                  file.string());
  });
}

void MaterialsXmlReader::processCreaturesNode(const pugi::xml_node &node,
                                              const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing creatures section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    // TODO: Load creatures from file - integrate with CreatureXmlReader
    spdlog::debug("[MaterialsXmlReader] Would load creature file: {}",
                  file.string());
  });
}

void MaterialsXmlReader::processItemsNode(const pugi::xml_node &node,
                                          const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing items section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    // TODO: Load items from file - integrate with existing item loading
    spdlog::debug("[MaterialsXmlReader] Would load item file: {}",
                  file.string());
  });
}

void MaterialsXmlReader::processTilesetsNode(const pugi::xml_node &node,
                                             const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing tilesets section");

  TilesetXmlReader reader(brushRegistry_, tilesetRegistry_);

  processIncludes(node, basePath, [&reader](const fs::path &file) {
    spdlog::debug("[MaterialsXmlReader] Loading tileset file: {}",
                  file.string());
    reader.loadTilesetFile(file);
  });
}

void MaterialsXmlReader::processPalettesNode(const pugi::xml_node &node,
                                             const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing palettes section");

  PaletteXmlReader reader(tilesetRegistry_, paletteRegistry_);

  processIncludes(node, basePath, [&reader](const fs::path &file) {
    spdlog::debug("[MaterialsXmlReader] Loading palette file: {}",
                  file.string());
    reader.load(file);
  });
}

void MaterialsXmlReader::processIncludes(
    const pugi::xml_node &node, const fs::path &basePath,
    std::function<void(const fs::path &)> fileProcessor) {

  for (pugi::xml_node const include : node.children("include")) {
    // Check for file include
    std::string file = include.attribute("file").as_string();
    if (!file.empty()) {
      fs::path const filePath = basePath / file;
      if (fs::exists(filePath)) {
        std::string const absPath = fs::absolute(filePath).string();
        if (loadedFiles_.find(absPath) == loadedFiles_.end()) {
          loadedFiles_.insert(absPath);
          fileProcessor(filePath);
        } else {
          spdlog::warn("[MaterialsXmlReader] Skipping already loaded: {}",
                       file);
        }
      } else {
        spdlog::warn("[MaterialsXmlReader] Include file not found: {}",
                     filePath.string());
      }
      continue;
    }

    // Check for folder include
    std::string const folder = include.attribute("folder").as_string();
    if (!folder.empty()) {
      fs::path const folderPath = basePath / folder;
      bool const recursive = include.attribute("subfolders").as_bool(false);

      if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
        auto files = collectXmlFiles(folderPath, recursive);
        spdlog::debug("[MaterialsXmlReader] Found {} XML files in {}",
                      files.size(), folderPath.string());

        for (const auto &xmlFile : files) {
          std::string const absPath = fs::absolute(xmlFile).string();
          if (loadedFiles_.find(absPath) == loadedFiles_.end()) {
            loadedFiles_.insert(absPath);
            fileProcessor(xmlFile);
          }
        }
      } else {
        spdlog::warn("[MaterialsXmlReader] Include folder not found: {}",
                     folderPath.string());
      }
    }
  }
}

std::vector<fs::path>
MaterialsXmlReader::collectXmlFiles(const fs::path &folder, bool recursive) {

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
    spdlog::error("[MaterialsXmlReader] Error scanning folder {}: {}",
                  folder.string(), e.what());
  }

  // Sort for consistent loading order
  std::sort(result.begin(), result.end());
  return result;
}

} // namespace MapEditor::IO
