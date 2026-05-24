#include "SimulationController.h"
#include "Services/ConfigService.h" // For TILE_SIZE
#include "UI/Map/MapPanel.h"
#include <cmath>

namespace MapEditor::AppLogic {

SimulationController::SimulationController(
    const Services::ViewSettings &view_settings)
    : view_settings_(view_settings) {}

void SimulationController::update(float delta_time, EditorSession *session,
                                  Services::ClientDataService *client_data,
                                  float zoom, const glm::vec2 &viewport_size,
                                  const glm::vec2 &camera_position,
                                  int16_t current_floor) const {
  if (!session)
    return;

  auto &simulator = session->getCreatureSimulator();

  // Update simulation enabled state from view settings
  bool const was_enabled = simulator.isEnabled();
  simulator.setEnabled(view_settings_.simulate_creatures);

  // Reset positions when simulation is toggled off
  if (was_enabled && !simulator.isEnabled()) {
    simulator.reset();
  }

  if (simulator.isEnabled()) {
    // Calculate viewport bounds for simulation
    float const tile_size_px = Config::Rendering::TILE_SIZE * zoom;

    int const tiles_x =
        static_cast<int>(std::ceil(viewport_size.x / tile_size_px)) + 2;
    int const tiles_y =
        static_cast<int>(std::ceil(viewport_size.y / tile_size_px)) + 2;

    Domain::Position const viewport_min(
        static_cast<int>(camera_position.x) - tiles_x / 2 - 1,
        static_cast<int>(camera_position.y) - tiles_y / 2 - 1, current_floor);
    Domain::Position const viewport_max(viewport_min.x + tiles_x + 2,
                                  viewport_min.y + tiles_y + 2, current_floor);

    // Execute simulation step
    simulator.update(delta_time, viewport_min, viewport_max, current_floor,
                     session->getMap(), client_data);
  }
}

void SimulationController::updateFromPanel(
    float delta_time, EditorSession *session,
    Services::ClientDataService *client_data, const UI::MapPanel &map_panel) {
  update(delta_time, session, client_data, map_panel.getZoom(),
         map_panel.getViewportSize(), map_panel.getCameraPosition(),
         static_cast<int16_t>(map_panel.getCurrentFloor()));
}

} // namespace MapEditor::AppLogic
