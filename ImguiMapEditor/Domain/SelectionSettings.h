#pragma once
#include "SelectionMode.h"

namespace MapEditor::Domain {

/**
 * Stores current selection preferences.
 * Serialization is handled by Services::SettingsRegistry.
 */
struct SelectionSettings {

  /// Floor scope for selection operations
  SelectionFloorScope floor_scope = SelectionFloorScope::CurrentFloor;

  /// Whether to use pixel-perfect selection (sprite hit testing)
  /// When false, uses Smart selection (logical priority)
  bool use_pixel_perfect = false;
};

} // namespace MapEditor::Domain
