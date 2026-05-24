#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace pugi {
class xml_node;
}

namespace MapEditor::Domain::Tileset {
class TilesetRegistry;
}

namespace MapEditor::Domain::Palette {
class PaletteRegistry;
}

namespace MapEditor::IO {

/**
 * Reads palette definitions from palettes.xml.
 *
 * palettes.xml structure:
 * <palettes>
 *   <palette name="Boss Encounters">
 *     <tileset>
 *       <include file="tilesets/creatures/Bosses.xml"/>
 *       <include file="tilesets/creatures/Magic.xml"/>
 *     </tileset>
 *   </palette>
 *   <palette name="All Creatures">
 *     <tileset>
 *       <include folder="tilesets/creatures/"/>
 *     </tileset>
 *   </palette>
 * </palettes>
 */
class PaletteXmlReader {
public:
  PaletteXmlReader(Domain::Tileset::TilesetRegistry &tilesetRegistry,
                   Domain::Palette::PaletteRegistry &paletteRegistry);

  /**
   * Load palettes from the given XML file.
   * @param path Path to palettes.xml
   * @return true if loading succeeded
   */
  bool load(const std::filesystem::path &path);

private:
  /**
   * Parse a single <palette> node and register it.
   */
  void parsePaletteNode(const pugi::xml_node &node,
                        const std::filesystem::path &basePath,
                        const std::filesystem::path &sourceFile);

  /**
   * Process <tileset><include .../></tileset> children.
   * Collects tileset names from includes.
   */
  std::vector<std::string>
  processTilesetIncludes(const pugi::xml_node &tilesetNode,
                         const std::filesystem::path &basePath);

  /**
   * Collect all XML files from a folder.
   */
  static std::vector<std::filesystem::path>
  collectXmlFiles(const std::filesystem::path &folder, bool recursive);

  /**
   * Extract tileset name from an XML file.
   * Returns empty string if not a valid tileset file.
   */
  static std::string getTilesetNameFromFile(const std::filesystem::path &file);

  Domain::Tileset::TilesetRegistry &tileset_registry_;
  Domain::Palette::PaletteRegistry &palette_registry_;
  std::unordered_set<std::string> loadedFiles_;
};

} // namespace MapEditor::IO
