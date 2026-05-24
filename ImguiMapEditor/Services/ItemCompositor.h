#pragma once
#include "Domain/ItemType.h"
// INTENTIONAL LAYER EXCEPTION: ItemCompositor produces Rendering::Texture
// objects for UI preview display. It composes sprite data into GPU-ready
// textures but delegates all GL operations to the Rendering layer.
#include "Rendering/Core/Texture.h"
#include <memory>
#include <unordered_map>

namespace MapEditor {

namespace IO {
class SprReader;
}

namespace Services {

/**
 * Composites multi-tile items into single textures for UI display.
 * 
 * For single-tile items (1x1): loads sprite directly from SprReader
 * For multi-tile items: composites all sprite parts into a single texture
 * Results are cached by client_id.
 */
class ItemCompositor {
public:
    /**
     * Create ItemCompositor with required dependencies.
     * @param spr_reader SprReader for raw sprite data access
     */
    explicit ItemCompositor(std::shared_ptr<IO::SprReader> spr_reader);
    ~ItemCompositor() = default;

    // Non-copyable
    ItemCompositor(const ItemCompositor&) = delete;
    ItemCompositor& operator=(const ItemCompositor&) = delete;

    /**
     * Get a composited texture for an item type.
     * For single-tile items: returns first sprite texture.
     * For multi-tile items: composites all sprite parts.
     * Results are cached by client_id.
     *
     * @param type ItemType to get texture for
     * @return Texture pointer, or nullptr if failed
     */
    Rendering::Texture* getCompositedItemTexture(const Domain::ItemType* type);

    /**
     * Clear the composited texture cache.
     */
    void clearCache();

    /**
     * Get number of cached textures.
     */
    size_t getCacheSize() const { return cache_.size(); }

private:
    std::shared_ptr<IO::SprReader> spr_reader_;
    
    // Composited item texture cache: client_id -> composited texture
    std::unordered_map<uint16_t, std::unique_ptr<Rendering::Texture>> cache_;
};

} // namespace Services
} // namespace MapEditor
