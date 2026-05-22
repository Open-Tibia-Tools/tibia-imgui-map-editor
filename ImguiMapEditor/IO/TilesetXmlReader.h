#pragma once

#include <filesystem>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace MapEditor::Brushes {
class BrushRegistry;
class IBrush;
}

namespace MapEditor::Domain::Tileset {
class Tileset;
class TilesetRegistry;
} // namespace MapEditor::Domain::Tileset

namespace MapEditor::IO {

/**
 * Reads tileset definitions from XML files.
 *
 * New format - standalone tileset files:
 * <tileset name="Undead">
 *   <brush name="skeleton"/>
 *   <creature name="ghost"/>
 *   <item id="1234"/>
 *   <separator name="Section"/>
 * </tileset>
 *
 * Entries are flat (no category subdivision).
 * Creatures support both:
 * - Reference by name: <creature name="skeleton"/>
 * - Inline definition: <creature name="skeleton" type="monster" looktype="33"/>
 */
class TilesetXmlReader {
public:
  TilesetXmlReader(Brushes::BrushRegistry &brushRegistry,
                   Domain::Tileset::TilesetRegistry &tilesetRegistry);
  ~TilesetXmlReader();

  /**
   * Load a single tileset XML file.
   * The file should have <tileset name="..."> as root.
   * @param path Path to the tileset XML file
   * @return true if loading succeeded
   */
  bool loadTilesetFile(const std::filesystem::path &path);

private:
  /**
   * Parse a <tileset> node and register it.
   * @param node The tileset XML node
   * @param sourceFile Path to the source file (for saving)
   */
  void parseTilesetNode(const pugi::xml_node &node,
                        const std::filesystem::path &sourceFile);

  /**
   * Parse child entries (brush, item, creature, separator) into a tileset.
   */
  void parseEntries(const pugi::xml_node &node,
                    Domain::Tileset::Tileset &tileset,
                    bool collectionTileset);

  void recordPlaceholder(std::string name);
  void decrementPlaceholderUsage(const std::string &name);
  void resolvePlaceholders(bool collectionTileset);

  Brushes::BrushRegistry &brush_registry_;
  Domain::Tileset::TilesetRegistry &tileset_registry_;
  std::unordered_set<std::string> loaded_files_;
  std::unordered_map<std::string, size_t> placeholder_usage_;
};

} // namespace MapEditor::IO
