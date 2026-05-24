#include "LightManager.h"
#include "LightColorPalette.h"
#include "Services/ClientDataService.h"
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <cstring> // For memcpy

namespace MapEditor {
namespace Rendering {

LightManager::LightManager(Services::ClientDataService* client_data)
    : client_data_(client_data)
{
}

LightManager::~LightManager() = default;

bool LightManager::initialize() {
    cache_ = std::make_unique<LightCache>();
    texture_ = std::make_unique<LightTexture>();
    overlay_ = std::make_unique<LightOverlay>();
    gatherer_ = std::make_unique<LightGatherer>();

    if (!texture_->initialize()) {
        spdlog::error("LightManager: Failed to initialize texture");
        return false;
    }
    if (!overlay_->initialize()) {
        spdlog::error("LightManager: Failed to initialize overlay");
        return false;
    }
    
    return true;
}

void LightManager::invalidateTile(int32_t x, int32_t y) {
    // Flag that we need to update the light texture next frame
    force_update_ = true;

    if (cache_) {
        // Invalidate chunk containing the tile AND its neighbors
        // to propagate light changes (spilling)
        
        // Using explicit shift to match ChunkedMap
        int32_t const cx = x >> 5;
        int32_t const cy = y >> 5;
        
        // Since we don't know the floor here yet (interface limitation), 
        // valid safe approach is to invalidate this column on all floors.
        // Most map edits are single floor, but 16 iterations is cheap.
        for (int16_t z = 0; z <= 15; ++z) {
            cache_->invalidateRegion(cx - 1, cy - 1, cx + 1, cy + 1, z);
        }
    }
}

void LightManager::invalidateAll() {
    force_update_ = true;
    if (cache_) {
        cache_->clear();
    }
}

void LightManager::render(const Domain::ChunkedMap& map,
                          int viewport_width, int viewport_height,
                          float camera_x, float camera_y, 
                          float zoom, int current_floor,
                          int start_floor, int end_floor,
                          const Domain::LightConfig& config)
{
    if (!config.enabled) return;

    // 1. Calculate tile bounds FIRST (to check against cache)
    float const tiles_x = viewport_width / (32.0f * zoom);
    float const tiles_y = viewport_height / (32.0f * zoom);
    
    int const start_x = static_cast<int>(std::floor(camera_x - tiles_x / 2)) - 1;
    int const end_x = static_cast<int>(std::ceil(camera_x + tiles_x / 2)) + 2;
    int const start_y = static_cast<int>(std::floor(camera_y - tiles_y / 2)) - 1;
    int const end_y = static_cast<int>(std::ceil(camera_y + tiles_y / 2)) + 2;
    
    int const width_tiles = end_x - start_x;
    int const height_tiles = end_y - start_y;
    
    if (width_tiles <= 0 || height_tiles <= 0) return;

    // OPTIMIZATION: Check if visible region, floor range, or config changed
    bool const bounds_changed =
        start_x != last_start_x_ ||
        start_y != last_start_y_ ||
        width_tiles != last_width_tiles_ ||
        height_tiles != last_height_tiles_ ||
        current_floor != last_floor_ ||
        start_floor != last_start_floor_ ||
        end_floor != last_end_floor_;

    bool const config_changed =
        config.ambient_color != last_config_.ambient_color ||
        config.ambient_level != last_config_.ambient_level ||
        config.enabled != last_config_.enabled;

    // If bounds and config match, we can reuse the texture
    // But we MUST recalculate screen coordinates because sub-pixel camera movement
    // changes where the texture is drawn, even if the integer tile range is same.
    if (!bounds_changed && !config_changed && !force_update_) {
        float const world_x = start_x * 32.0f;
        float const world_y = start_y * 32.0f;
        float const screen_x = (world_x - camera_x * 32.0f) * zoom + viewport_width / 2.0f;
        float const screen_y = (world_y - camera_y * 32.0f) * zoom + viewport_height / 2.0f;
        float const screen_w = width_tiles * 32.0f * zoom;
        float const screen_h = height_tiles * 32.0f * zoom;

        overlay_->apply(texture_->getTextureId(),
                       glm::vec4(screen_x, screen_y, screen_w, screen_h),
                       glm::vec2(viewport_width, viewport_height));
        return;
    }

    // Update state
    last_start_x_ = start_x;
    last_start_y_ = start_y;
    last_width_tiles_ = width_tiles;
    last_height_tiles_ = height_tiles;
    last_floor_ = current_floor;
    last_start_floor_ = start_floor;
    last_end_floor_ = end_floor;
    last_config_ = config;
    force_update_ = false;

    // 2. Prepare buffer
    size_t const required_size = width_tiles * height_tiles * 4;
    if (viewport_buffer_.size() < required_size) {
        viewport_buffer_.resize(required_size);
    }
    
    // 3. Iterate over chunks in the view
    // To optimized cache usage, iterate by chunks visible
    int const chunk_start_x = start_x >> 5;
    int const chunk_start_y = start_y >> 5;
    int const chunk_end_x = end_x >> 5;
    int const chunk_end_y = end_y >> 5;

    for (int cy = chunk_start_y; cy <= chunk_end_y; ++cy) {
        for (int cx = chunk_start_x; cx <= chunk_end_x; ++cx) {
            
            // Get or compute the grid
            // Use current_floor as cache key since all lights are projected to this floor
            CachedLightGrid& grid = cache_->getOrCreateGrid(cx, cy, static_cast<int16_t>(current_floor));
            if (!grid.is_valid) {
                gatherer_->clear();
                
                // Use multi-floor gathering if we have a floor range
                if (start_floor != end_floor) {
                    gatherer_->gatherForChunkMultiFloor(
                        map, cx, cy, client_data_,
                        static_cast<int16_t>(start_floor),
                        static_cast<int16_t>(end_floor));
                } else {
                    // Single floor mode
                    gatherer_->gatherForChunk(map, cx, cy, client_data_, static_cast<int16_t>(current_floor));
                }
                
                computeChunkLight(grid, gatherer_->getLights(), config, cx, cy);
                grid.is_valid = true;
            }
            
            // Copy relevant part of the grid to viewport buffer
            int const chunk_pixel_x = cx * 32;
            int const chunk_pixel_y = cy * 32;
            
            // Intersection between Chunk and Viewport
            int const ix_start = std::max(start_x, chunk_pixel_x);
            int const ix_end = std::min(end_x, chunk_pixel_x + 32);
            int const iy_start = std::max(start_y, chunk_pixel_y);
            int const iy_end = std::min(end_y, chunk_pixel_y + 32);
            
            if (ix_start >= ix_end || iy_start >= iy_end) continue;
            
            // Copy loops
            for (int y = iy_start; y < iy_end; ++y) {
                int const dest_y = y - start_y;
                int const src_y = y - chunk_pixel_y;
                
                int const dest_row_start = (dest_y * width_tiles + (ix_start - start_x)) * 4;
                int const src_row_start = (src_y * 32 + (ix_start - chunk_pixel_x)) * 4; // Assuming 32 width
                int const row_len = (ix_end - ix_start) * 4; // Bytes
                
                // Safety check
                 std::memcpy(&viewport_buffer_[dest_row_start], 
                            reinterpret_cast<const uint8_t*>(&grid.pixels[0]) + src_row_start,
                            row_len);
            }
        }
    }

    // 4. Upload and Render
    texture_->upload(viewport_buffer_, width_tiles, height_tiles);
    
    // Calc screen rect
    float const world_x = start_x * 32.0f; // TILE_SIZE
    float const world_y = start_y * 32.0f;
    
    float const screen_x = (world_x - camera_x * 32.0f) * zoom + viewport_width / 2.0f;
    float const screen_y = (world_y - camera_y * 32.0f) * zoom + viewport_height / 2.0f;
    float const screen_w = width_tiles * 32.0f * zoom;
    float const screen_h = height_tiles * 32.0f * zoom;
    
    overlay_->apply(texture_->getTextureId(),
                   glm::vec4(screen_x, screen_y, screen_w, screen_h),
                   glm::vec2(viewport_width, viewport_height));
}

void LightManager::computeChunkLight(CachedLightGrid& grid,
                                     const std::vector<Domain::LightSource>& lights,
                                     const Domain::LightConfig& config,
                                     int32_t chunk_x, int32_t chunk_y)
{
    // OPTIMIZED: Iterate lights first, then only affected tiles
    
    // Get ambient light color
    float ambient_r, ambient_g, ambient_b;
    LightColorPalette::from8bitFloat(config.ambient_color, ambient_r, ambient_g, ambient_b);
    
    float const ambient_scale = config.ambient_level / 255.0f;
    ambient_r *= ambient_scale;
    ambient_g *= ambient_scale;
    ambient_b *= ambient_scale;
    
    uint8_t const base_r = static_cast<uint8_t>(ambient_r * 255.0f);
    uint8_t const base_g = static_cast<uint8_t>(ambient_g * 255.0f);
    uint8_t const base_b = static_cast<uint8_t>(ambient_b * 255.0f);
    
    // Fill with ambient first
    uint32_t const ambient_packed = base_r | (base_g << 8) | (base_b << 16) | (255 << 24);
    std::fill(grid.pixels.begin(), grid.pixels.end(), ambient_packed);
    
    int const chunk_start_x = chunk_x * 32;
    int const chunk_start_y = chunk_y * 32;
    
    // Iterate lights FIRST, then only tiles affected by each light
    for (const auto& light : lights) {
        // Pre-compute light color once per light
        float lr, lg, lb;
        LightColorPalette::from8bitFloat(light.color, lr, lg, lb);
        
        // Calculate bounding box of affected tiles (in local chunk coords)
        int const radius = light.intensity;
        int const min_x = std::max(0, light.x - radius - chunk_start_x);
        int const max_x = std::min(31, light.x + radius - chunk_start_x);
        int const min_y = std::max(0, light.y - radius - chunk_start_y);
        int const max_y = std::min(31, light.y + radius - chunk_start_y);
        
        // Skip if light doesn't affect this chunk
        if (min_x > 31 || max_x < 0 || min_y > 31 || max_y < 0) continue;
        
        // Only iterate tiles within light's bounding box
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                int const tile_x = chunk_start_x + x;
                int const tile_y = chunk_start_y + y;
                
                float const dx = (static_cast<float>(tile_x) + 0.5f) - (static_cast<float>(light.x) + 0.5f);
                float const dy = (static_cast<float>(tile_y) + 0.5f) - (static_cast<float>(light.y) + 0.5f);
                float const dist_sq = dx*dx + dy*dy;
                
                // Skip tiles outside actual radius (bounding box overestimates)
                float const radius_sq = static_cast<float>(radius * radius);
                if (dist_sq > radius_sq) continue;
                
                float const distance = std::sqrt(dist_sq);
                float intensity = (-distance + static_cast<float>(light.intensity)) * 0.2f;
                
                if (intensity < 0.01f) continue;
                if (intensity > 1.0f) intensity = 1.0f;
                
                // Update pixel with max blending
                uint32_t& pixel = grid.pixels[y * 32 + x];
                int const r = std::max(static_cast<int>(pixel & 0xFF), static_cast<int>(lr * intensity * 255.0f));
                int const g = std::max(static_cast<int>((pixel >> 8) & 0xFF), static_cast<int>(lg * intensity * 255.0f));
                int const b = std::max(static_cast<int>((pixel >> 16) & 0xFF), static_cast<int>(lb * intensity * 255.0f));
                
                pixel = static_cast<uint32_t>(std::min(r, 255)) |
                       (static_cast<uint32_t>(std::min(g, 255)) << 8) |
                       (static_cast<uint32_t>(std::min(b, 255)) << 16) |
                       (255 << 24);
            }
        }
    }
}

} // namespace Rendering
} // namespace MapEditor
