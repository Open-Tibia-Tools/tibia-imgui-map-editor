#pragma once
#include "Domain/Position.h"
#include <glm/glm.hpp>

namespace MapEditor {
namespace Domain {

/**
 * Interface for coordinate transformation between screen and tile coordinates.
 * Enables testing and allows different coordinate systems.
 */
class ICoordinateTransformer {
public:
    virtual ~ICoordinateTransformer() = default;

    /**
     * Convert screen coordinates to tile position.
     * @param screen_pos Position in screen pixels
     * @return Tile position in world coordinates
     */
    virtual Domain::Position screenToTile(const glm::vec2& screen_pos) const = 0;

    /**
     * Convert tile position to screen coordinates.
     * @param tile_pos Tile position in world coordinates
     * @return Position in screen pixels (top-left corner of tile)
     */
    virtual glm::vec2 tileToScreen(const Domain::Position& tile_pos) const = 0;

    /**
     * Get current camera position in world coordinates.
     */
    virtual glm::vec2 getCameraPosition() const = 0;

    /**
     * Get current zoom level.
     */
    virtual float getZoom() const = 0;

    /**
     * Get current floor being viewed.
     */
    virtual int getCurrentFloor() const = 0;

    /**
     * Get the viewport position in screen pixels (top-left corner).
     */
    virtual glm::vec2 getViewportPos() const = 0;

    /**
     * Get the viewport size in screen pixels.
     */
    virtual glm::vec2 getViewportSize() const = 0;
};

} // namespace Domain
} // namespace MapEditor
