/**
 * @file BrushEnums.cpp
 * @brief Implementation of XML string ↔ enum conversion functions.
 */

#include "BrushEnums.h"
#include <unordered_map>

namespace MapEditor::Brushes {

// ═══════════════════════════════════════════════════════════════════════════
// EdgeType Conversions
// ═══════════════════════════════════════════════════════════════════════════

EdgeType parseEdgeName(std::string_view name) {
  static const std::unordered_map<std::string_view, EdgeType> map = {
      {"n", EdgeType::N},          {"e", EdgeType::E},
      {"s", EdgeType::S},          {"w", EdgeType::W},
      {"cnw", EdgeType::CNW},      {"cne", EdgeType::CNE},
      {"csw", EdgeType::CSW},      {"cse", EdgeType::CSE},
      {"dnw", EdgeType::DNW},      {"dne", EdgeType::DNE},
      {"dse", EdgeType::DSE},      {"dsw", EdgeType::DSW},
      {"center", EdgeType::Center}};
  auto it = map.find(name);
  return it != map.end() ? it->second : EdgeType::None;
}

std::string_view edgeTypeToString(EdgeType type) {
  switch (type) {
  case EdgeType::N:
    return "n";
  case EdgeType::E:
    return "e";
  case EdgeType::S:
    return "s";
  case EdgeType::W:
    return "w";
  case EdgeType::CNW:
    return "cnw";
  case EdgeType::CNE:
    return "cne";
  case EdgeType::CSW:
    return "csw";
  case EdgeType::CSE:
    return "cse";
  case EdgeType::DNW:
    return "dnw";
  case EdgeType::DNE:
    return "dne";
  case EdgeType::DSE:
    return "dse";
  case EdgeType::DSW:
    return "dsw";
  case EdgeType::Center:
    return "center";
  default:
    return "";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// TableAlign Conversions
// ═══════════════════════════════════════════════════════════════════════════

TableAlign parseTableAlign(std::string_view name) {
  static const std::unordered_map<std::string_view, TableAlign> map = {
      {"alone", TableAlign::Alone},      {"north", TableAlign::North},
      {"south", TableAlign::South},      {"east", TableAlign::East},
      {"west", TableAlign::West},        {"horizontal", TableAlign::Horizontal},
      {"vertical", TableAlign::Vertical}};
  auto it = map.find(name);
  return it != map.end() ? it->second : TableAlign::Alone;
}

std::string_view tableAlignToString(TableAlign align) {
  switch (align) {
  case TableAlign::Alone:
    return "alone";
  case TableAlign::North:
    return "north";
  case TableAlign::South:
    return "south";
  case TableAlign::East:
    return "east";
  case TableAlign::West:
    return "west";
  case TableAlign::Horizontal:
    return "horizontal";
  case TableAlign::Vertical:
    return "vertical";
  default:
    return "alone";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// WallAlign Conversions (handles spaced strings like "east T")
// ═══════════════════════════════════════════════════════════════════════════

WallAlign parseWallType(std::string_view name) {
  static const std::unordered_map<std::string_view, WallAlign> map = {
      {"pole", WallAlign::Pole},
      {"corner", WallAlign::Corner},
      {"horizontal", WallAlign::Horizontal},
      {"vertical", WallAlign::Vertical},
      {"north end", WallAlign::NorthEnd},
      {"south end", WallAlign::SouthEnd},
      {"east end", WallAlign::EastEnd},
      {"west end", WallAlign::WestEnd},
      {"north T", WallAlign::NorthT},
      {"south T", WallAlign::SouthT},
      {"east T", WallAlign::EastT},
      {"west T", WallAlign::WestT},
      {"intersection", WallAlign::Intersection},
      {"northwest diagonal", WallAlign::NorthwestDiagonal},
      {"northeast diagonal", WallAlign::NortheastDiagonal},
      {"southwest diagonal", WallAlign::SouthwestDiagonal},
      {"southeast diagonal", WallAlign::SoutheastDiagonal}};
  auto it = map.find(name);
  return it != map.end() ? it->second : WallAlign::Pole;
}

std::string_view wallAlignToString(WallAlign align) {
  switch (align) {
  case WallAlign::Pole:
    return "pole";
  case WallAlign::NorthwestDiagonal:
    return "corner";
  case WallAlign::SouthEnd:
    return "south end";
  case WallAlign::EastEnd:
    return "east end";
  case WallAlign::NortheastDiagonal:
    return "northeast diagonal";
  case WallAlign::Horizontal:
    return "horizontal";
  case WallAlign::SouthT:
    return "south T";
  case WallAlign::NorthEnd:
    return "north end";
  case WallAlign::Vertical:
    return "vertical";
  case WallAlign::SouthwestDiagonal:
    return "southwest diagonal";
  case WallAlign::EastT:
    return "east T";
  case WallAlign::SoutheastDiagonal:
    return "southeast diagonal";
  case WallAlign::WestEnd:
    return "west end";
  case WallAlign::NorthT:
    return "north T";
  case WallAlign::WestT:
    return "west T";
  case WallAlign::Intersection:
    return "intersection";
  case WallAlign::Untouchable:
    return "untouchable";
  default:
    return "pole";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DoorType Conversions
// ═══════════════════════════════════════════════════════════════════════════

DoorType parseDoorType(std::string_view name) {
  static const std::unordered_map<std::string_view, DoorType> map = {
      {"archway", DoorType::Archway}, {"normal", DoorType::Normal},
      {"locked", DoorType::Locked},   {"quest", DoorType::Quest},
      {"magic", DoorType::Magic},     {"normal_alt", DoorType::NormalAlt},
      {"window", DoorType::Window},   {"hatch_window", DoorType::HatchWindow}};
  auto it = map.find(name);
  return it != map.end() ? it->second : DoorType::Undefined;
}

std::string_view doorTypeToString(DoorType type) {
  switch (type) {
  case DoorType::Archway:
    return "archway";
  case DoorType::Normal:
    return "normal";
  case DoorType::Locked:
    return "locked";
  case DoorType::Quest:
    return "quest";
  case DoorType::Magic:
    return "magic";
  case DoorType::NormalAlt:
    return "normal_alt";
  case DoorType::Window:
    return "window";
  case DoorType::HatchWindow:
    return "hatch_window";
  default:
    return "";
  }
}

} // namespace MapEditor::Brushes
