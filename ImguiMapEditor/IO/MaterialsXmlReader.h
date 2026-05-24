#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace pugi {
class xml_node;
}

namespace MapEditor::Brushes {
class BrushRegistry;
}

namespace MapEditor::Domain::Tileset {
class TilesetRegistry;
}

namespace MapEditor::Domain::Palette {
class PaletteRegistry;
}

namespace MapEditor::IO {

/**
 * Reads the materials.xml file and orchestrates loading of all material types.
 *
 * materials.xml structure:
 * <materials>
 *   <borders><include folder="borders/"/></borders>
 *   <brushes><include folder="brushes/"/></brushes>
 *   <creatures><include folder="creatures/"/></creatures>
 *   <items><include folder="items/"/></items>
 *   <tilesets><include folder="tilesets/" subfolders="true"/></tilesets>
 *   <palettes><include file="palettes.xml"/></palettes>
 * </materials>
 *
 * Supports:
 * - <include file="path/to/file.xml"/> - Load single file
 * - <include folder="path/to/folder/"/> - Load all XML files in folder
 * - <include folder="path/to/folder/" subfolders="true"/> - Recursive loading
 */
class MaterialsXmlReader {
public:
  MaterialsXmlReader(Brushes::BrushRegistry &brushRegistry,
                     Domain::Tileset::TilesetRegistry &tilesetRegistry,
                     Domain::Palette::PaletteRegistry &paletteRegistry);

  /**
   * Load all materials from the given materials.xml file.
   * @param path Path to materials.xml
   * @return true if loading succeeded
   */
  bool load(const std::filesystem::path &path);

private:
  // Section processors
  void processBordersNode(const pugi::xml_node &node,
                          const std::filesystem::path &basePath);
  void processBrushesNode(const pugi::xml_node &node,
                          const std::filesystem::path &basePath);
  void processCreaturesNode(const pugi::xml_node &node,
                            const std::filesystem::path &basePath);
  void processItemsNode(const pugi::xml_node &node,
                        const std::filesystem::path &basePath);
  void processTilesetsNode(const pugi::xml_node &node,
                           const std::filesystem::path &basePath);
  void processPalettesNode(const pugi::xml_node &node,
                           const std::filesystem::path &basePath);

  /**
   * Process include directives within a node.
   * @param node Parent node containing include children
   * @param basePath Base path for relative file resolution
   * @param fileProcessor Callback to process each loaded XML file
   */
  void processIncludes(
      const pugi::xml_node &node, const std::filesystem::path &basePath,
      std::function<void(const std::filesystem::path &)> fileProcessor);

  /**
   * Collect all XML files from a folder.
   * @param folder Folder to scan
   * @param recursive Whether to scan subfolders
   * @return List of XML file paths
   */
  static std::vector<std::filesystem::path>
  collectXmlFiles(const std::filesystem::path &folder, bool recursive);

  Brushes::BrushRegistry &brushRegistry_;
  Domain::Tileset::TilesetRegistry &tilesetRegistry_;
  Domain::Palette::PaletteRegistry &paletteRegistry_;
  std::unordered_set<std::string> loadedFiles_; // Prevent circular includes
};

} // namespace MapEditor::IO
