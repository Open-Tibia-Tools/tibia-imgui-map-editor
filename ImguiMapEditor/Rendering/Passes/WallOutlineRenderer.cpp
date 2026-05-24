#include "Rendering/Passes/WallOutlineRenderer.h"
#include "Core/Config.h"
#include "Rendering/Resources/ShaderLoader.h"
#include "Rendering/Visibility/FloorIterator.h"
#include "Services/ClientDataService.h"
#include "Services/ViewSettings.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

WallOutlineRenderer::WallOutlineRenderer(
    Services::ClientDataService *client_data)
    : client_data_(client_data) {}

WallOutlineRenderer::~WallOutlineRenderer() = default;

bool WallOutlineRenderer::initialize() {
  if (initialized_)
    return true;

  // Load shader using the convenient load() method that handles paths
  shader_ = ShaderLoader::load("color_overlay");
  if (!shader_) {
    spdlog::error("WallOutlineRenderer: Failed to load color_overlay shader");
    return false;
  }

  // Create quad VAO/VBO
  quad_vao_.create();
  quad_vbo_.create();

  glBindVertexArray(quad_vao_.get());
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_.get());

  // Vertex format: x, y, r, g, b, a (6 floats per vertex)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  // Create line VAO/VBO
  line_vao_.create();
  line_vbo_.create();

  glBindVertexArray(line_vao_.get());
  glBindBuffer(GL_ARRAY_BUFFER, line_vbo_.get());

  // Same vertex format for lines
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  initialized_ = true;
  spdlog::debug("WallOutlineRenderer initialized");
  return true;
}

bool WallOutlineRenderer::isBlockingGround(const Domain::ItemType &type) {
  // Blocking ground: Unpassable + BlockMissiles + NOT Moveable + top_order==0 +
  // NOT FullTile
  return type.hasFlag(Domain::ItemFlag::Unpassable) &&
         type.hasFlag(Domain::ItemFlag::BlockMissiles) &&
         !type.hasFlag(Domain::ItemFlag::Moveable) && type.top_order == 0 &&
         !type.hasFlag(Domain::ItemFlag::FullTile);
}

bool WallOutlineRenderer::isWallItem(const Domain::ItemType &type) {
  // Yellow lines: Unpassable + BlockMissiles + NOT Moveable + top_order!=0
  return type.hasFlag(Domain::ItemFlag::Unpassable) &&
         type.hasFlag(Domain::ItemFlag::BlockMissiles) &&
         !type.hasFlag(Domain::ItemFlag::Moveable) && type.top_order != 0;
}

bool WallOutlineRenderer::tileHasWall(const Domain::ChunkedMap &map, int x,
                                      int y, int z) const {
  Domain::Position const pos(x, y, static_cast<int16_t>(z));
  const Domain::Tile *tile = map.getTile(pos);
  if (!tile)
    return false;

  for (const auto &item : tile->getItems()) {
    const Domain::ItemType *type =
        client_data_ ? client_data_->getItemTypeByServerId(item->getServerId())
                     : nullptr;
    if (type && isWallItem(*type)) {
      return true;
    }
  }
  return false;
}

void WallOutlineRenderer::addQuad(float x, float y, float w, float h, float r,
                                  float g, float b, float a) {
  // Two triangles forming a quad
  // Triangle 1: top-left, top-right, bottom-right
  quad_vertices_.insert(quad_vertices_.end(), {
                                                  x,
                                                  y,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                                  x + w,
                                                  y,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                                  x + w,
                                                  y + h,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                              });
  // Triangle 2: top-left, bottom-right, bottom-left
  quad_vertices_.insert(quad_vertices_.end(), {
                                                  x,
                                                  y,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                                  x + w,
                                                  y + h,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                                  x,
                                                  y + h,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                              });
}

void WallOutlineRenderer::addLine(float x1, float y1, float x2, float y2,
                                  float r, float g, float b, float a) {
  line_vertices_.insert(line_vertices_.end(), {
                                                  x1,
                                                  y1,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                                  x2,
                                                  y2,
                                                  r,
                                                  g,
                                                  b,
                                                  a,
                                              });
}

void WallOutlineRenderer::collectData(const Domain::ChunkedMap &map,
                                      int start_x, int start_y, int end_x,
                                      int end_y, int floor_z,
                                      float floor_offset) {
  line_vertices_.clear();
  quad_vertices_.clear();

  if (!client_data_)
    return;

  // Reserve some space
  line_vertices_.reserve(Config::Performance::WALL_VERTICES_RESERVE);

  // Scan all visible tiles for wall items
  for (int y = start_y; y < end_y; ++y) {
    for (int x = start_x; x < end_x; ++x) {
      Domain::Position const pos(x, y, static_cast<int16_t>(floor_z));
      const Domain::Tile *tile = map.getTile(pos);
      if (!tile)
        continue;

      float const screen_x = x * TILE_SIZE - floor_offset;
      float const screen_y = y * TILE_SIZE - floor_offset;

      bool has_wall = false;

      // Check all stacked items on this tile for walls
      for (const auto &item : tile->getItems()) {
        const Domain::ItemType *type =
            client_data_->getItemTypeByServerId(item->getServerId());
        if (type && isWallItem(*type)) {
          has_wall = true;
          break;
        }
      }

      // Add yellow lines for wall connections
      // Only check +X and +Y to avoid duplicates
      if (has_wall) {
        float const center_x = screen_x + TILE_SIZE / 2.0f;
        float const center_y = screen_y + TILE_SIZE / 2.0f;

        // Check neighbor at (x+1, y)
        if (tileHasWall(map, x + 1, y, floor_z)) {
          float const neighbor_center_x =
              (x + 1) * TILE_SIZE - floor_offset + TILE_SIZE / 2.0f;
          addLine(center_x, center_y, neighbor_center_x, center_y, YELLOW_R,
                  YELLOW_G, YELLOW_B, YELLOW_A);
        }

        // Check neighbor at (x, y+1)
        if (tileHasWall(map, x, y + 1, floor_z)) {
          float const neighbor_center_y =
              (y + 1) * TILE_SIZE - floor_offset + TILE_SIZE / 2.0f;
          addLine(center_x, center_y, center_x, neighbor_center_y, YELLOW_R,
                  YELLOW_G, YELLOW_B, YELLOW_A);
        }
      }
    }
  }
}

void WallOutlineRenderer::render(const RenderContext &context) {
  // Check generic wall outline toggle
  if (!context.view_settings || !context.view_settings->show_wall_outline) {
    return;
  }

  // Only render on the current floor
  float const floor_offset = FloorIterator::getFloorOffset(context.current_floor,
                                                     context.current_floor);

  const auto &map = context.map;
  const auto start_x = context.visible_bounds.start_x;
  const auto start_y = context.visible_bounds.start_y;
  const auto end_x = context.visible_bounds.end_x;
  const auto end_y = context.visible_bounds.end_y;
  const auto floor_z = context.current_floor;
  const auto &mvp = context.mvp_matrix;

  if (!initialized_)
    return;

  // Check cache
  bool cache_valid = (map.getRevision() == last_revision_) &&
                     (floor_z == last_floor_) && (start_x == last_start_x_) &&
                     (start_y == last_start_y_) && (end_x == last_end_x_) &&
                     (end_y == last_end_y_) &&
                     (floor_offset == last_floor_offset_);

  if (!cache_valid) {
    // Collect overlay data
    collectData(map, start_x, start_y, end_x, end_y, floor_z, floor_offset);

    // Update cache state
    last_revision_ = map.getRevision();
    last_floor_ = floor_z;
    last_start_x_ = start_x;
    last_start_y_ = start_y;
    last_end_x_ = end_x;
    last_end_y_ = end_y;
    last_floor_offset_ = floor_offset;

    // Upload to VBOs (GL_STATIC_DRAW since we cache it until change)
    if (!quad_vertices_.empty()) {
      glBindVertexArray(quad_vao_.get());
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_.get());
      glBufferData(GL_ARRAY_BUFFER, quad_vertices_.size() * sizeof(float),
                   quad_vertices_.data(), GL_STATIC_DRAW);
    }

    if (!line_vertices_.empty()) {
      glBindVertexArray(line_vao_.get());
      glBindBuffer(GL_ARRAY_BUFFER, line_vbo_.get());
      glBufferData(GL_ARRAY_BUFFER, line_vertices_.size() * sizeof(float),
                   line_vertices_.data(), GL_STATIC_DRAW);
    }
  }

  // Debug output
  static int frame_count = 0;
  if (++frame_count % 120 == 0) { // Log every 2 seconds at 60fps
    spdlog::debug(
        "WallOutlineRenderer: {} quad verts, {} line verts (cached: {})",
        quad_vertices_.size(), line_vertices_.size(), cache_valid);
  }

  // Early exit if nothing to draw
  if (quad_vertices_.empty() && line_vertices_.empty())
    return;

  // Setup blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  shader_->use();
  shader_->setMat4("uMVP", mvp);

  // Draw quads (orange overlays)
  if (!quad_vertices_.empty()) {
    glBindVertexArray(quad_vao_.get());
    // No buffer update needed here, it's done in cache check
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(quad_vertices_.size() / 6));
  }

  // Draw lines (yellow wall connections)
  if (!line_vertices_.empty()) {
    glLineWidth(
        Config::Rendering::WALL_OUTLINE_WIDTH); // Make lines more visible
    glBindVertexArray(line_vao_.get());
    // No buffer update needed here
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_vertices_.size() / 6));
    glLineWidth(1.0f); // Reset
  }

  glBindVertexArray(0);
}

} // namespace Rendering
} // namespace MapEditor
