#pragma once

#include "Core/Config.h"
#include "Domain/ICoordinateTransformer.h"
#include "Domain/Position.h"
#include "Presentation/NotificationHelper.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>
#include <string>


namespace MapEditor {
namespace Rendering {

/**
 * Renders the status information overlay (coordinates, selection count, FPS).
 */
class StatusOverlay {
public:
  StatusOverlay() = default;

  static void render(ImDrawList *draw_list, const Domain::ICoordinateTransformer &camera,
              size_t selection_count, bool is_hovered,
              float framerate);
};

} // namespace Rendering
} // namespace MapEditor
