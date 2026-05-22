#pragma once

#include "Tileset.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace MapEditor::Domain::Tileset {

/**
 * Registry of all loaded tilesets.
 *
 * Tilesets are registered when loading from XML files.
 * The registry owns the Tileset objects.
 *
 * NOTE: This class is NOT a singleton. It should be owned by TilesetService
 * and injected where needed per project dependency injection rules.
 */
class TilesetRegistry {
public:
  TilesetRegistry() = default;
  ~TilesetRegistry() = default;

  // Non-copyable, movable
  TilesetRegistry(const TilesetRegistry &) = delete;
  TilesetRegistry &operator=(const TilesetRegistry &) = delete;
  TilesetRegistry(TilesetRegistry &&) = default;
  TilesetRegistry &operator=(TilesetRegistry &&) = default;

  void registerTileset(std::unique_ptr<Tileset> tileset) {
    if (tileset) {
      tilesetOrder_.push_back(tileset->getName());
      tilesets_.push_back(std::move(tileset));
    }
  }

  void clear() {
    tilesets_.clear();
    tilesetOrder_.clear();
  }

  const std::vector<std::unique_ptr<Tileset>> &getAllTilesets() const {
    return tilesets_;
  }

  Tileset *getTileset(const std::string &name) const {
    auto it = std::find_if(
        tilesets_.begin(), tilesets_.end(),
        [&name](const auto &tileset) { return tileset->getName() == name; });
    return it != tilesets_.end() ? it->get() : nullptr;
  }

  Tileset *getTilesetBySourceFile(const std::filesystem::path &path) const {
    const auto absolutePath = std::filesystem::absolute(path);
    auto it = std::find_if(tilesets_.begin(), tilesets_.end(),
                           [&absolutePath](const auto &tileset) {
                             return std::filesystem::absolute(
                                        tileset->getSourceFile()) ==
                                    absolutePath;
                           });
    return it != tilesets_.end() ? it->get() : nullptr;
  }

  /**
   * Get all tileset names in registration order.
   */
  const std::vector<std::string> &getTilesetNames() const {
    return tilesetOrder_;
  }

  size_t size() const { return tilesets_.size(); }
  bool empty() const { return tilesets_.empty(); }

private:
  std::vector<std::unique_ptr<Tileset>> tilesets_;
  std::vector<std::string> tilesetOrder_; // Maintain insertion order
};

} // namespace MapEditor::Domain::Tileset
