#pragma once
#include "Domain/ICoordinateTransformer.h"
#include "Domain/Position.h"
#include <glm/glm.hpp>

namespace MapEditor {
namespace UI {

/**
 * Manages camera state and coordinate transformations.
 * Implements ICoordinateTransformer interface.
 * 
 * Single responsibility: camera position, zoom, floor, and coordinate transforms.
 */
class MapViewCamera : public Domain::ICoordinateTransformer {
public:
    MapViewCamera();
    ~MapViewCamera() override = default;

    // ICoordinateTransformer interface
    Domain::Position screenToTile(const glm::vec2& screen_pos) const override;
    glm::vec2 tileToScreen(const Domain::Position& tile_pos) const override;
    glm::vec2 getCameraPosition() const override { return camera_pos_; }
    float getZoom() const override { return zoom_; }
    int getCurrentFloor() const override { return current_floor_; }

    // Camera position
    void setCameraPosition(float x, float y);
    void setCameraCenter(const Domain::Position& pos);
    void setCameraCenter(int32_t x, int32_t y, int16_t z);
    
    Domain::Position getCameraCenter() const;

    // Zoom control
    void setZoom(float zoom);
    void adjustZoom(float delta, const glm::vec2& pivot_screen);

    // Floor control
    void setCurrentFloor(int16_t floor);
    void floorUp();
    void floorDown();

    // Viewport (must be set each frame)
    void setViewport(const glm::vec2& pos, const glm::vec2& size);
    glm::vec2 getViewportPos() const { return viewport_pos_; }
    glm::vec2 getViewportSize() const { return viewport_size_; }

    // Map bounds (clamp camera to prevent scrolling past map edges)
    void setMapBounds(int32_t max_x, int32_t max_y);

private:
    glm::vec2 camera_pos_{500.0f, 500.0f};
    int16_t current_floor_ = 7; // FLOOR_GROUND
    float zoom_ = 1.0f;

    glm::vec2 viewport_pos_{0, 0};
    glm::vec2 viewport_size_{800, 600};

    int32_t map_max_x_ = 0;
    int32_t map_max_y_ = 0;


    // Removed static constexpr constants - now in Config.h
};

} // namespace UI
} // namespace MapEditor
