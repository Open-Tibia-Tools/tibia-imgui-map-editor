#pragma once
/**
 * @file BrushXmlReader.h
 * @brief Reads brush definitions from RME-format XML files.
 *
 * Supports parsing of ground, wall, doodad, table, and carpet brushes
 * from the standard RME brushes.xml format.
 */

#include <filesystem>
#include <pugixml.hpp>
#include <string>
#include <unordered_set>

namespace MapEditor::Brushes {
class BrushRegistry;
}

namespace MapEditor::Services {
class ClientDataService;
}

namespace MapEditor::Services::Brushes {
class BorderLookupService;
class WallLookupService;
class TableLookupService;
class CarpetLookupService;
} // namespace MapEditor::Services::Brushes

namespace MapEditor::IO {

/**
 * Reads brush definitions from RME-format XML files.
 *
 * RME XML format example:
 * <brushes>
 *   <brush name="grass" type="ground" server_lookid="4526" z-order="1100">
 *     <item id="4526" chance="50"/>
 *     <border id="1" to="water">
 *       <borderitem edge="n" id="4609"/>
 *       ...
 *     </border>
 *   </brush>
 *   <brush name="stone_wall" type="wall" server_lookid="1">
 *     <wall type="horizontal">
 *       <item id="100"/>
 *     </wall>
 *     ...
 *   </brush>
 * </brushes>
 */
class BrushXmlReader {
public:
  struct Dependencies {
    MapEditor::Brushes::BrushRegistry *brushRegistry = nullptr;
    Services::Brushes::BorderLookupService *borderLookup = nullptr;
    Services::Brushes::WallLookupService *wallLookup = nullptr;
    Services::Brushes::TableLookupService *tableLookup = nullptr;
    Services::Brushes::CarpetLookupService *carpetLookup = nullptr;
    Services::ClientDataService *clientData = nullptr;
  };

  explicit BrushXmlReader(Dependencies deps);

  /**
   * Load a single brush XML file.
   * @param path Path to brush XML file
   * @return true if loading succeeded
   */
  bool loadFile(const std::filesystem::path &path);

  /**
   * Load all XML files from a directory.
   * @param dir Directory containing brush XML files
   * @return Number of files loaded successfully
   */
  size_t loadDirectory(const std::filesystem::path &dir);

  /**
   * Get count of brushes loaded in last operation.
   */
  size_t getLastLoadCount() const { return lastLoadCount_; }

private:
  void parseBrushesRoot(const pugi::xml_node &root,
                        const std::filesystem::path &sourceFile);
  void parseBrush(const pugi::xml_node &node,
                  const std::filesystem::path &sourceFile);

  // Type-specific parsers
  void parseGroundBrush(const pugi::xml_node &node, const std::string &name,
                        uint32_t lookId,
                        const std::filesystem::path &sourceFile);
  void parseWallBrush(const pugi::xml_node &node, const std::string &name,
                      uint32_t lookId,
                      const std::filesystem::path &sourceFile);
  void parseWallDecorationBrush(const pugi::xml_node &node,
                                const std::string &name, uint32_t lookId,
                                const std::filesystem::path &sourceFile);
  void parseDoodadBrush(const pugi::xml_node &node, const std::string &name,
                        uint32_t lookId,
                        const std::filesystem::path &sourceFile);
  void parseTableBrush(const pugi::xml_node &node, const std::string &name,
                       uint32_t lookId,
                       const std::filesystem::path &sourceFile);
  void parseCarpetBrush(const pugi::xml_node &node, const std::string &name,
                        uint32_t lookId,
                        const std::filesystem::path &sourceFile);

  Dependencies deps_;
  std::unordered_set<std::string> loadedFiles_;
  size_t lastLoadCount_ = 0;
};

} // namespace MapEditor::IO
