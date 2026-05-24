#pragma once
#include "Application/EditorSession.h"
#include "Services/ClientDataService.h"
#include "Services/CreatureSimulator.h"
#include "Services/ViewSettings.h"
#include <glm/vec2.hpp>

// Forward declarations
namespace MapEditor::UI {
class MapPanel;
}

namespace MapEditor::AppLogic {

class SimulationController {
public:
  explicit SimulationController(const Services::ViewSettings &view_settings);

  /**
   * Updates the creature simulation for the active session.
   * Calculates the visible viewport from the provided camera parameters to
   * optimize simulation.
   */
  void update(float delta_time, EditorSession *session,
              Services::ClientDataService *client_data, float zoom,
              const glm::vec2 &viewport_size, const glm::vec2 &camera_position,
              int16_t current_floor) const;

  /**
   * Simplified update that extracts camera parameters from MapPanel.
   * Reduces parameter passing at call site.
   */
  void updateFromPanel(float delta_time, EditorSession *session,
                       Services::ClientDataService *client_data,
                       const UI::MapPanel &map_panel);

private:
  const Services::ViewSettings &view_settings_;
};

} // namespace MapEditor::AppLogic
