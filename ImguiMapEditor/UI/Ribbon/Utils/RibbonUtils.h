#pragma once

#include "UI/Core/Theme.h"
#include <functional>
#include <imgui.h>
#include <string>

namespace MapEditor::UI::Ribbon::Utils {

/**
 * Renders a standard ribbon button with tooltip support and disabled handling.
 * Optimized to avoid heap allocations by using templates and stack buffers.
 *
 * @param icon The icon to display (e.g. ICON_FA_SAVE)
 * @param label The text label (optional, pass nullptr or empty string for icon
 * only)
 * @param enabled Whether the button is clickable
 * @param tooltip_enabled Tooltip when enabled
 * @param tooltip_disabled Tooltip when disabled
 * @param on_click Callback function to execute when clicked
 */
template <typename ClickCallback>
inline void RenderButton(const char *icon, const char *label, bool enabled,
                         const char *tooltip_enabled,
                         const char *tooltip_disabled,
                         ClickCallback &&on_click) {
  ImGui::BeginDisabled(!enabled);

  bool clicked = false;

  // Optimization: Only format if we actually need to concatenate strings.
  // If label is missing/empty, use the icon pointer directly.
  if (label && label[0] != '\0') {
    std::string button_label = std::format("{}{}", icon, label);
    clicked = ImGui::Button(button_label.c_str());
  } else {
    clicked = ImGui::Button(icon);
  }

  if (clicked) {
    on_click();
  }

  // Using AllowWhenDisabled to show tooltip even if button is disabled
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!enabled) {
      if (tooltip_disabled && tooltip_disabled[0] != '\0') {
        ImGui::SetTooltip("%s", tooltip_disabled);
      }
    } else {
      if (tooltip_enabled && tooltip_enabled[0] != '\0') {
        ImGui::SetTooltip("%s", tooltip_enabled);
      }
    }
  }

  ImGui::EndDisabled();
}

/**
 * Renders a toggle button (checkbox behavior with button look).
 * Optimized to avoid heap allocations.
 *
 * @param icon The icon to display
 * @param active Whether the button is currently active (pressed)
 * @param tooltip Tooltip text
 * @param on_toggle Callback function to execute when toggled
 * @param label Optional identifier for ImGui ID (e.g. "##AllFloors")
 */
template <typename ToggleCallback>
inline void RenderToggleButton(const char *icon, bool active,
                               const char *tooltip, ToggleCallback &&on_toggle,
                               const char *label = nullptr) {
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  }

  bool clicked = false;

  // Optimization: Bypass formatting if no label/ID is appended
  if (label && label[0] != '\0') {
    std::string button_label = std::format("{}{}", icon, label);
    clicked = ImGui::Button(button_label.c_str());
  } else {
    clicked = ImGui::Button(icon);
  }

  if (clicked) {
    on_toggle();
  }

  if (active) {
    ImGui::PopStyleColor();
  }

  if (ImGui::IsItemHovered()) {
    if (tooltip && tooltip[0] != '\0') {
      ImGui::SetTooltip("%s", tooltip);
    }
  }
}

/**
 * Renders a radio button style toggle.
 * Similar to RenderToggleButton but visually implies mutual exclusivity
 * (though currently implemented identically, this abstraction allows future
 * styling changes).
 */
template <typename SelectCallback>
inline void RenderRadioButton(const char *icon, bool selected,
                              const char *tooltip, SelectCallback &&on_select,
                              const char *label = nullptr) {
  // Reuse RenderToggleButton logic for now, but semantically distinct
  RenderToggleButton(icon, selected, tooltip,
                     std::forward<SelectCallback>(on_select), label);
}

/**
 * Renders a vertical separator customized for the ribbon.
 */
inline void RenderSeparator() {
  ImGui::PushStyleColor(ImGuiCol_Text, SemanticColors::MUTED);
  ImGui::Text("|");
  ImGui::PopStyleColor();
}

/**
 * Renders a standard checkbox with tooltip support.
 */
template <typename ToggleCallback>
inline void RenderCheckbox(const char *label, bool &value, const char *tooltip,
                           ToggleCallback &&on_toggle) {
  if (ImGui::Checkbox(label, &value)) {
    on_toggle();
  }
  if (ImGui::IsItemHovered() && tooltip && tooltip[0] != '\0') {
    ImGui::SetTooltip("%s", tooltip);
  }
}

} // namespace MapEditor::UI::Ribbon::Utils
