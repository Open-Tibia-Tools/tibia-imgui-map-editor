#include "MaterialsXmlReader.h"

#include "Brushes/BrushRegistry.h"
#include "Brushes/Data/BorderBlock.h"
#include "Brushes/Enums/BrushEnums.h"
#include "Brushes/Types/CreatureBrush.h"
#include "IO/BrushXmlReader.h"
#include "IO/PaletteXmlReader.h"
#include "IO/TilesetXmlReader.h"
#include "IO/XmlUtils.h"
#include "Services/ClientDataService.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor::IO {

namespace fs = std::filesystem;

MaterialsXmlReader::MaterialsXmlReader(
    Brushes::BrushRegistry &brushRegistry,
    Domain::Tileset::TilesetRegistry &tilesetRegistry,
    Domain::Palette::PaletteRegistry &paletteRegistry,
    Services::Brushes::BorderLookupService *borderLookup,
    Services::Brushes::WallLookupService *wallLookup,
    Services::Brushes::TableLookupService *tableLookup,
    Services::Brushes::CarpetLookupService *carpetLookup,
    Services::ClientDataService *clientData)
    : brushRegistry_(brushRegistry), tilesetRegistry_(tilesetRegistry),
      paletteRegistry_(paletteRegistry), borderLookup_(borderLookup),
      wallLookup_(wallLookup), tableLookup_(tableLookup),
      carpetLookup_(carpetLookup), clientData_(clientData) {}

bool MaterialsXmlReader::load(const fs::path &path) {
  if (!fs::exists(path)) {
    spdlog::error("[MaterialsXmlReader] File not found: {}", path.string());
    return false;
  }

  pugi::xml_document doc;
  std::string error;
  pugi::xml_node root = XmlUtils::loadXmlFile(path, "materials", doc, error);

  if (!root) {
    spdlog::error("[MaterialsXmlReader] {}", error);
    return false;
  }

  fs::path basePath = path.parent_path();
  loadedFiles_.clear();
  loadedFiles_.insert(fs::absolute(path).string());

  spdlog::info("[MaterialsXmlReader] Loading materials from: {}",
               path.string());

  // Process sections in dependency order
  for (pugi::xml_node child : root.children()) {
    std::string nodeName = child.name();

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
    pugi::xml_document doc;
    std::string error;
    const auto root = XmlUtils::loadXmlFile(file, "materials", doc, error);
    if (!root) {
      spdlog::warn("[MaterialsXmlReader] {}", error);
      return;
    }

    for (const auto borderNode : root.children("border")) {
      const auto borderId = borderNode.attribute("id").as_uint(0);
      if (borderId == 0) {
        continue;
      }

      Brushes::BorderBlock block;
      block.setGroup(borderNode.attribute("group").as_uint(0));
      for (const auto borderItem : borderNode.children("borderitem")) {
        const auto edge = Brushes::parseEdgeName(borderItem.attribute("edge").as_string());
        const auto itemId = borderItem.attribute("item").as_uint(
            borderItem.attribute("id").as_uint(0));
        const auto chance = borderItem.attribute("chance").as_uint(1);
        if (edge != Brushes::EdgeType::None && itemId != 0) {
          block.addItem(edge, itemId, chance);
        }
      }
      brushRegistry_.registerBorderTemplate(borderId, std::move(block));
    }
  });
}

void MaterialsXmlReader::processBrushesNode(const pugi::xml_node &node,
                                            const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing brushes section");

  BrushXmlReader reader({&brushRegistry_, borderLookup_, wallLookup_, tableLookup_,
                         carpetLookup_, clientData_});

  processIncludes(node, basePath,
                  [&reader](const fs::path &file) { reader.loadFile(file); });
}

void MaterialsXmlReader::processCreaturesNode(const pugi::xml_node &node,
                                              const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing creatures section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    if (clientData_) {
      if (clientData_->loadCreatureData(file)) {
        for (const auto &creature : clientData_->getCreatures()) {
          if (!creature) {
            continue;
          }

          auto *brush = brushRegistry_.getBrush(creature->name);
          if (!brush) {
            auto creatureBrush = std::make_unique<Brushes::CreatureBrush>(
                creature->name, creature->outfit);
            brush = creatureBrush.get();
            brushRegistry_.addBrush(std::move(creatureBrush));
          }
          brushRegistry_.registerCreatureBinding(creature->name, brush);
        }
      }
    }
  });
}

void MaterialsXmlReader::processItemsNode(const pugi::xml_node &node,
                                          const fs::path &basePath) {
  spdlog::debug("[MaterialsXmlReader] Processing items section");

  processIncludes(node, basePath, [this](const fs::path &file) {
    if (clientData_) {
      clientData_->loadItemData(file);
    }
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

  for (pugi::xml_node include : node.children("include")) {
    // Check for file include
    std::string file = include.attribute("file").as_string();
    if (!file.empty()) {
      fs::path filePath = basePath / file;
      if (fs::exists(filePath)) {
        std::string absPath = fs::absolute(filePath).string();
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
    std::string folder = include.attribute("folder").as_string();
    if (!folder.empty()) {
      fs::path folderPath = basePath / folder;
      bool recursive = include.attribute("subfolders").as_bool(false);

      if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
        auto files = collectXmlFiles(folderPath, recursive);
        spdlog::debug("[MaterialsXmlReader] Found {} XML files in {}",
                      files.size(), folderPath.string());

        for (const auto &xmlFile : files) {
          std::string absPath = fs::absolute(xmlFile).string();
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
