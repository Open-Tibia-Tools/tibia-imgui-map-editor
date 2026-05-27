#include "MainWindow.h"
#include "Rendering/Frame/RenderingManager.h"
#include "Services/ClipboardService.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Presentation {

MainWindow::MainWindow(Services::ViewSettings &view_settings,
                       Services::ClientVersionRegistry &version_registry,
                       UI::MapPanel &map_panel,
                       UI::IngameBoxWindow &ingame_box_window,
                       MenuBar &menu_bar, AppLogic::MapTabManager *tab_manager)
    : view_settings_(view_settings), version_registry_(version_registry),
      map_panel_(map_panel), ingame_box_window_(ingame_box_window),
      menu_bar_(menu_bar), tab_manager_(tab_manager) {}

void MainWindow::openPropertiesDialog(Domain::Item *item) {
  if (!item)
    return;

  properties_dialog_.open(item, [this]() {
    // Mark session as modified when properties change
    if (tab_manager_ && tab_manager_->getActiveSession()) {
      tab_manager_->getActiveSession()->setModified(true);
    }
  });
}

void MainWindow::openSpawnPropertiesDialog(Domain::Spawn *spawn,
                                           const Domain::Position &pos) {
  if (!spawn)
    return;

  spawn_properties_dialog_.open(spawn, pos, [this]() {
    // Mark session as modified when spawn properties change
    if (tab_manager_ && tab_manager_->getActiveSession()) {
      tab_manager_->getActiveSession()->setModified(true);
    }
  });
}

void MainWindow::openCreaturePropertiesDialog(
    Domain::Creature *creature, const std::string &name,
    const Domain::Position &creature_pos) {
  if (!creature)
    return;

  creature_properties_dialog_.open(creature, name, creature_pos, [this]() {
    // Mark session as modified when creature properties change
    if (tab_manager_ && tab_manager_->getActiveSession()) {
      tab_manager_->getActiveSession()->setModified(true);
    }
  });
}

void MainWindow::renderEditor(Domain::ChunkedMap *current_map,
                              Rendering::RenderingManager *rendering_manager,
                              const Rendering::AnimationTicks *anim_ticks) {
  auto *map_renderer =
      rendering_manager ? rendering_manager->getRenderer() : nullptr;

  // Main menu bar
  if (ImGui::BeginMainMenuBar()) {
    menu_bar_.render();
    ImGui::EndMainMenuBar();
  }

  // Dockspace for the main viewport
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

  // Render each open map as a separate dockable window
  if (tab_manager_ && tab_manager_->getTabCount() > 0) {
    for (int i = 0; i < tab_manager_->getTabCount(); ++i) {
      auto *session = tab_manager_->getSession(i);
      if (!session)
        continue;

      // Create unique window name using SESSION ID
      auto session_id = session->getID();
      std::string window_name = session->getDisplayName() + "###MapSession" +
                                std::to_string(session_id);

      bool is_open = true;
      ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

      // Custom Tab Styling logic
      bool is_modified = session->isModified();
      bool is_active = (tab_manager_->getActiveTabIndex() == i);
      int pushed_colors = 0;

      if (is_modified || is_active) {
          ImVec4 tab_color;
          if (is_modified) {
              if (is_active) {
                  // Pulsate between Green and Yellow for Active + Modified
                  float t = (sinf((float)ImGui::GetTime() * 5.0f) * 0.5f) + 0.5f;
                  ImVec4 color_green = ImVec4(0.0f, 0.5f, 0.0f, 0.7f);
                  ImVec4 color_yellow = ImVec4(1.0f, 0.8f, 0.0f, 0.7f);

                  tab_color.x = color_green.x + (color_yellow.x - color_green.x) * t;
                  tab_color.y = color_green.y + (color_yellow.y - color_green.y) * t;
                  tab_color.z = color_green.z + (color_yellow.z - color_green.z) * t;
                  tab_color.w = 0.7f;
              } else {
                  // Static yellow/gold for Modified
                  tab_color = ImVec4(0.8f, 0.65f, 0.0f, 0.7f);
              }
          } else {
              // Static green for Active
              tab_color = ImVec4(0.0f, 0.5f, 0.0f, 0.7f);
          }

          ImGui::PushStyleColor(ImGuiCol_Tab, tab_color);
          ImGui::PushStyleColor(ImGuiCol_TabActive, tab_color);
          ImGui::PushStyleColor(ImGuiCol_TabHovered, tab_color);
          pushed_colors = 3;
      }

      if (ImGui::Begin(window_name.c_str(), &is_open, window_flags)) {
        // Check if this window was focused
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            tab_manager_->getActiveTabIndex() != i) {
          tab_manager_->setActiveTab(i);
        }

        // Only render map content for active map (to save resources)
        if (tab_manager_->getActiveTabIndex() == i) {
          // Lighting controls toolbar (per-map)
          ImGui::Checkbox("Enable Lighting",
                          &view_settings_.map_lighting_enabled);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(120);
          ImGui::SliderInt("Ambient", &view_settings_.map_ambient_light, 0,
                           255);
          ImGui::Separator();

          // Render the map panel with explicit animation ticks
          if (rendering_manager && map_renderer) {
            auto *state = rendering_manager->getRenderState(session->getID());
            if (state) {
              map_panel_.render(session->getMap(), *state, map_renderer,
                                anim_ticks);
            } else {
              spdlog::error("Error: RenderState not found for session {}",
                            static_cast<int>(session->getID()));
              ImGui::TextColored(ImVec4(1, 0, 0, 1),
                                 "Error: RenderState not found for session %d",
                                 static_cast<int>(session->getID()));
            }
          }

          // Check if context menu should be shown
          if (map_panel_.shouldShowContextMenu()) {
            context_menu_.show(map_panel_.getContextMenuPosition());
            map_panel_.clearContextMenuFlag();
          }

          // Render context menu (call each frame)
          context_menu_.render(
              session, clipboard_,
              [this](Domain::Item *item) {
                // Properties callback - open dialog
                properties_dialog_.open(item, [this]() {
                  // Mark session as modified when properties change
                  if (tab_manager_ && tab_manager_->getActiveSession()) {
                    tab_manager_->getActiveSession()->setModified(true);
                  }
                });
              },
              [this](const Domain::Position &dest) {
                map_panel_.setCameraCenter(dest);
              });

          // Render properties dialogs if open
          properties_dialog_.render();
          spawn_properties_dialog_.render();
          creature_properties_dialog_.render();
        } else {
          // Just show placeholder for inactive maps
          ImGui::TextDisabled("Click to activate this map");
        }
      }
      ImGui::End();

      if (pushed_colors > 0) {
          ImGui::PopStyleColor(pushed_colors);
      }


      // Handle window close
      if (!is_open) {
        // Use callback for deferred close to prevent use-after-free
        if (on_close_requested_) {
          on_close_requested_(i);
        } else {
          tab_manager_->closeTab(i);
        }
        break; // Exit loop since indices changed
      }
    }
  } else {
    // No maps open - show empty "Map" window
    if (ImGui::Begin("Map")) {
      ImGui::TextDisabled("No maps open. Use File > New or File > Open.");
    }
    ImGui::End();
  }


  // Always render Ingame Box Preview Window when in editor
  if (current_map) {
    Domain::Position cursor_pos = map_panel_.getCameraCenter();

    if (tab_manager_ && tab_manager_->getActiveSession()) {
      auto &selection = tab_manager_->getActiveSession()->getSelectionService();
      if (!selection.isEmpty()) {
        auto positions = selection.getPositions();
        if (!positions.empty()) {
          cursor_pos = positions.front();
        }
      }
    }

    ingame_box_window_.render(current_map, map_renderer, view_settings_,
                              cursor_pos, &view_settings_.show_ingame_box);
  }
}

} // namespace Presentation
} // namespace MapEditor
