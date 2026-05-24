#include "Rendering/Passes/BackgroundRenderer.h"
// Note: stb_image may already be implemented elsewhere in the project.
// If there are linker errors about duplicate symbols, remove this
// implementation block.
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#endif
#include <stb_image.h>

#include <filesystem>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

bool BackgroundRenderer::tryLoad() {
  if (load_attempted_) {
    return texture_.isValid();
  }
  load_attempted_ = true;

  // Look for background.jpg in data/ folder next to executable
  std::filesystem::path const exe_path = std::filesystem::current_path();
  std::filesystem::path const bg_path = exe_path / "data" / "background.jpg";

  if (!std::filesystem::exists(bg_path)) {
    spdlog::debug("Background image not found: {}", bg_path.string());
    return false;
  }

  // Load image with stb_image
  int width, height, channels;
  unsigned char *data =
      stbi_load(bg_path.string().c_str(), &width, &height, &channels, 4);

  if (!data) {
    spdlog::warn("Failed to load background image: {}", stbi_failure_reason());
    return false;
  }

  // Create texture from loaded data
  texture_ = Texture(static_cast<uint32_t>(width),
                     static_cast<uint32_t>(height), data);

  stbi_image_free(data);

  spdlog::info("Loaded background image: {}x{}", width, height);
  return texture_.isValid();
}

void BackgroundRenderer::render() {
  if (!isLoaded()) {
    return;
  }

  // Get viewport size
  ImGuiIO  const&io = ImGui::GetIO();
  ImVec2 const viewport_size = io.DisplaySize;

  // Get background draw list (renders behind everything)
  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

  // Stretch image to fill entire viewport
  ImTextureID const tex_id = (ImTextureID)(intptr_t)texture_.id();
  ImVec2 const p_min(0.0f, 0.0f);
  ImVec2 const p_max(viewport_size.x, viewport_size.y);
  draw_list->AddImage(tex_id, p_min, p_max);
}

} // namespace Rendering
} // namespace MapEditor
