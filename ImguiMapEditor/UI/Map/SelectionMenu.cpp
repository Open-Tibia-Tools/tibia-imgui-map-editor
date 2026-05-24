#include "SelectionMenu.h"
#include "Application/EditorSession.h"
#include "Input/Hotkeys.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>

namespace MapEditor::UI {

SelectionMenu::SelectionMenu(Domain::SelectionSettings &settings)
    : settings_(settings) {}

void SelectionMenu::render(AppLogic::EditorSession *session) {
  if (ImGui::BeginMenu("Selection")) {
    renderSelectionActions(session);
    ImGui::Separator();
    renderSelectionModeOptions();
    ImGui::Separator();
    renderFloorScopeOptions();
    ImGui::EndMenu();
  }
}

void SelectionMenu::renderSelectionActions(AppLogic::EditorSession *session) {
  // Note: Select All menu item removed - not needed per user feedback

  if (ImGui::MenuItem(ICON_FA_XMARK " Deselect", "Esc", false,
                      session != nullptr)) {
    if (session) {
      session->clearSelection();
    }
  }
}

void SelectionMenu::renderSelectionModeOptions() {
  bool const is_smart = !settings_.use_pixel_perfect;
  bool const is_pixel_perfect = settings_.use_pixel_perfect;

  if (ImGui::MenuItem(ICON_FA_WAND_MAGIC_SPARKLES " Smart Selection", nullptr,
                      is_smart)) {
    settings_.use_pixel_perfect = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Context-sensitive logical selection\nPriority: Creature "
                      "> Top Item > Ground");
  }

  if (ImGui::MenuItem(ICON_FA_CROSSHAIRS " Pixel Perfect", nullptr,
                      is_pixel_perfect)) {
    settings_.use_pixel_perfect = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Use sprite hit testing to select\nthe exact item under cursor");
  }
}

void SelectionMenu::renderFloorScopeOptions() {
  using Scope = Domain::SelectionFloorScope;

  bool const is_current = (settings_.floor_scope == Scope::CurrentFloor);
  bool const is_visible = (settings_.floor_scope == Scope::VisibleFloors);
  bool const is_all = (settings_.floor_scope == Scope::AllFloors);

  if (ImGui::MenuItem(ICON_FA_LAYER_GROUP " Current Floor", nullptr,
                      is_current)) {
    settings_.floor_scope = Scope::CurrentFloor;
  }

  if (ImGui::MenuItem(ICON_FA_EYE " Visible Floors", nullptr, is_visible)) {
    settings_.floor_scope = Scope::VisibleFloors;
  }

  if (ImGui::MenuItem(ICON_FA_CUBES " All Floors (0-15)", nullptr, is_all)) {
    settings_.floor_scope = Scope::AllFloors;
  }
}

} // namespace MapEditor::UI
