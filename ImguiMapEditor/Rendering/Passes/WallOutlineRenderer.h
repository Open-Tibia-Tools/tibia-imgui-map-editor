#pragma once
#include "Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "Domain/ItemType.h"
#include "Rendering/Core/GLHandle.h"
#include "Rendering/Core/IRenderPass.h"
#include "Rendering/Core/Shader.h"
#include <glm/glm.hpp>
#include <vector>

namespace MapEditor {

namespace Services {
class ClientDataService;
}

namespace Rendering {

/**
 * Renders wall outline overlays on the map viewport.
 *
 * Two types of overlays:
 * 1. Orange semi-transparent boxes on "blocking ground" tiles
 *    (Unpassable + BlockMissiles + !Moveable + top_order==0)
 * 2. Yellow lines connecting adjacent "wall" tiles
 *    (Unpassable + BlockMissiles + !Moveable + top_order!=0)
 *
 * Rendered as an overlay after sprite batch, with blending enabled.
 */
class WallOutlineRenderer : public IRenderPass {
public:
  WallOutlineRenderer(Services::ClientDataService *client_data);
  ~WallOutlineRenderer() override;

  // Non-copyable
  WallOutlineRenderer(const WallOutlineRenderer &) = delete;
  WallOutlineRenderer &operator=(const WallOutlineRenderer &) = delete;

  /**
   * Initialize GPU resources (shader, VAO, VBOs).
   * @return true if successful
   */
  bool initialize();

  /**
   * Render wall outlines for visible tiles (yellow lines only).
   * Call AFTER sprite batch rendering.
   */
  void render(const RenderContext &context) override;

private:
  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;

  // Colors (RGBA)
  static constexpr float ORANGE_R = Config::Rendering::WALL_HOOK_COLOR_R;
  static constexpr float ORANGE_G = Config::Rendering::WALL_HOOK_COLOR_G;
  static constexpr float ORANGE_B = Config::Rendering::WALL_HOOK_COLOR_B;
  static constexpr float ORANGE_A = Config::Rendering::WALL_HOOK_COLOR_A;

  static constexpr float YELLOW_R = Config::Rendering::WALL_CONN_COLOR_R;
  static constexpr float YELLOW_G = Config::Rendering::WALL_CONN_COLOR_G;
  static constexpr float YELLOW_B = Config::Rendering::WALL_CONN_COLOR_B;
  static constexpr float YELLOW_A = Config::Rendering::WALL_CONN_COLOR_A;

  /**
   * Check if an item type qualifies as "blocking ground" (orange overlay).
   */
  static bool isBlockingGround(const Domain::ItemType &type) ;

  /**
   * Check if an item type qualifies as "wall" (yellow lines).
   */
  static bool isWallItem(const Domain::ItemType &type) ;

  /**
   * Check if a tile at (x, y, z) contains a wall item.
   */
  bool tileHasWall(const Domain::ChunkedMap &map, int x, int y, int z) const;

  /**
   * Collect overlay data by scanning visible tiles.
   */
  void collectData(const Domain::ChunkedMap &map, int start_x, int start_y,
                   int end_x, int end_y, int floor_z, float floor_offset);

  /**
   * Add a quad (2 triangles) to the vertex buffer.
   */
  void addQuad(float x, float y, float w, float h, float r, float g, float b,
               float a);

  /**
   * Add a line segment to the line vertex buffer.
   */
  void addLine(float x1, float y1, float x2, float y2, float r, float g,
               float b, float a);

  // Services (not owned)
  Services::ClientDataService *client_data_ = nullptr;

  // Shader
  std::unique_ptr<Shader> shader_;

  // GPU resources
  DeferredVAOHandle quad_vao_;
  DeferredVBOHandle quad_vbo_;
  DeferredVAOHandle line_vao_;
  DeferredVBOHandle line_vbo_;

  // Per-frame vertex data
  std::vector<float> quad_vertices_; // x, y, r, g, b, a per vertex
  std::vector<float> line_vertices_; // x, y, r, g, b, a per vertex

  bool initialized_ = false;

  // Cache tracking
  uint32_t last_revision_ = 0;
  int last_floor_ = -128;
  int last_start_x_ = -1;
  int last_start_y_ = -1;
  int last_end_x_ = -1;
  int last_end_y_ = -1;
  float last_floor_offset_ = 0.0f;
};

} // namespace Rendering
} // namespace MapEditor
