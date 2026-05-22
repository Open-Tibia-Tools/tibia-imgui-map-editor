#include "BrushesPanel.h"
#include "Brushes/BrushController.h"
#include "IconsFontAwesome6.h"
#include "Services/BrushSettingsService.h"
#include "UI/Ribbon/Utils/RibbonUtils.h"
#include <algorithm>
#include <cstdio>
#include <format>
#include <imgui.h>
#include <string>
#include <string_view>

namespace MapEditor {
namespace UI {
namespace Ribbon {

namespace {

[[nodiscard]] bool supportsPreviewBorder(const Brushes::IBrush *brush) {
  if (!brush) {
    return false;
  }

  if (brush->hasCollection()) {
    return true;
  }

  switch (brush->getType()) {
  case Brushes::BrushType::Raw:
  case Brushes::BrushType::Ground:
  case Brushes::BrushType::Wall:
  case Brushes::BrushType::WallDecoration:
  case Brushes::BrushType::Table:
  case Brushes::BrushType::Carpet:
  case Brushes::BrushType::Door:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] bool supportsLockDoors(const Brushes::IBrush *brush) {
  return brush && brush->getType() == Brushes::BrushType::Door;
}

[[nodiscard]] bool supportsSpawnSettings(const Brushes::IBrush *brush) {
  return brush &&
         (brush->getType() == Brushes::BrushType::Spawn ||
          brush->getType() == Brushes::BrushType::Creature);
}

[[nodiscard]] bool supportsHouseAssignment(const Brushes::IBrush *brush) {
  return brush &&
         (brush->getType() == Brushes::BrushType::House ||
          brush->getType() == Brushes::BrushType::HouseExit);
}

[[nodiscard]] bool supportsWaypointName(const Brushes::IBrush *brush) {
  return brush && brush->getType() == Brushes::BrushType::Waypoint;
}

[[nodiscard]] int resolveSelectedBrushId(
    Brushes::BrushController *controller) {
  if (!controller) {
    return -1;
  }

  if (controller->isCurrentBrush(controller->getSpawnBrush())) {
    return 2;
  }
  if (controller->isCurrentBrush(controller->getPZBrush())) {
    return 3;
  }
  if (controller->isCurrentBrush(controller->getNoPvpBrush())) {
    return 4;
  }
  if (controller->isCurrentBrush(controller->getNoLogoutBrush())) {
    return 5;
  }
  if (controller->isCurrentBrush(controller->getPvpZoneBrush())) {
    return 6;
  }
  if (controller->isCurrentBrush(controller->getEraserBrush())) {
    return 7;
  }
  if (controller->isCurrentBrush(controller->getHouseBrush())) {
    return 8;
  }
  if (controller->isCurrentBrush(controller->getWaypointBrush())) {
    return 9;
  }
  if (controller->isCurrentBrush(controller->getHouseExitBrush())) {
    return 10;
  }
  if (controller->isCurrentBrush(controller->getOptionalBorderBrush())) {
    return 11;
  }
  if (controller->isCurrentBrush(controller->getNormalDoorBrush())) {
    return 12;
  }
  if (controller->isCurrentBrush(controller->getLockedDoorBrush())) {
    return 13;
  }
  if (controller->isCurrentBrush(controller->getQuestDoorBrush())) {
    return 14;
  }
  if (controller->isCurrentBrush(controller->getMagicDoorBrush())) {
    return 15;
  }
  if (controller->isCurrentBrush(controller->getArchwayBrush())) {
    return 16;
  }
  if (controller->isCurrentBrush(controller->getWindowBrush()) ||
      controller->isCurrentBrush(controller->getHatchWindowBrush())) {
    return 17;
  }
  if (controller->isCurrentBrush(controller->getNormalAltDoorBrush())) {
    return 18;
  }

  const auto *currentBrush = controller->getCurrentBrush();
  if (!currentBrush) {
    return -1;
  }

  switch (currentBrush->getType()) {
  case Brushes::BrushType::Ground:
    return 0;
  case Brushes::BrushType::Raw:
    return 1;
  default:
    return -1;
  }
}

} // namespace

BrushesPanel::BrushesPanel(Brushes::BrushController *controller,
                           Services::BrushSettingsService *settingsService)
    : controller_(controller), settingsService_(settingsService) {}

void BrushesPanel::Render() {
  selected_brush_ = resolveSelectedBrushId(controller_);
  const auto *currentBrush = controller_ ? controller_->getCurrentBrush() : nullptr;

  Utils::RenderToggleButton(
      ICON_FA_PAINTBRUSH, selected_brush_ == 0, "Paint ground tiles (G)",
      [this]() { selected_brush_ = 0; }, " Ground");

  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_PEN, selected_brush_ == 1, "Paint raw items (walls, objects) (R)",
      [this]() { selected_brush_ = 1; }, " Raw");

  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_LOCATION_DOT, selected_brush_ == 2, "Place spawn points (S)",
      [this]() {
        selected_brush_ = 2;
        if (controller_) {
          controller_->activateSpawnBrush();
        }
      },
      " Spawn");

  ImGui::SameLine();

  if (supportsSpawnSettings(currentBrush) && settingsService_) {
    bool autoSpawn = settingsService_->getAutoCreateSpawn();
    if (ImGui::Checkbox("##AutoSpawn", &autoSpawn)) {
      settingsService_->setAutoCreateSpawn(autoSpawn);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Auto-create spawn when placing creatures");
    }
    ImGui::SameLine();

    ImGui::Text(ICON_FA_CIRCLE_NOTCH);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Spawn radius follows the current brush size");
    }
    ImGui::SameLine();

    ImGui::Text("%d", settingsService_->getStandardSize());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Spawn radius: %d tiles",
                        settingsService_->getStandardSize());
    }
    ImGui::SameLine();

    ImGui::Text(ICON_FA_CLOCK);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Spawn timer (seconds)");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(60.0f);
    int time = settingsService_->getDefaultSpawnTime();
    if (ImGui::InputInt("##SpawnTime", &time, 0, 0)) {
      settingsService_->setDefaultSpawnTime(std::clamp(time, 1, 86400));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Spawn timer: %d seconds", time);
    }
    ImGui::SameLine();
  }

  Utils::RenderToggleButton(
      ICON_FA_SHIELD_HALVED, selected_brush_ == 3, "Protection Zone flag (PZ)",
      [this]() {
        selected_brush_ = 3;
        if (controller_) {
          controller_->activatePZBrush();
        }
      },
      " PZ");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_HAND, selected_brush_ == 4, "No PvP Zone flag",
      [this]() {
        selected_brush_ = 4;
        if (controller_) {
          controller_->activateNoPvpBrush();
        }
      },
      " NoPvP");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_DOOR_CLOSED, selected_brush_ == 5, "No Logout Zone flag",
      [this]() {
        selected_brush_ = 5;
        if (controller_) {
          controller_->activateNoLogoutBrush();
        }
      },
      " NoLog");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_SKULL, selected_brush_ == 6, "PvP Zone flag",
      [this]() {
        selected_brush_ = 6;
        if (controller_) {
          controller_->activatePvpZoneBrush();
        }
      },
      " PvP");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_ERASER, selected_brush_ == 7,
      "Eraser - remove items from tiles (E)",
      [this]() {
        selected_brush_ = 7;
        if (controller_) {
          controller_->activateEraserBrush();
        }
      },
      " Eraser");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_HOUSE, selected_brush_ == 8, "House - assign tiles to houses (H)",
      [this]() {
        selected_brush_ = 8;
        if (controller_) {
          controller_->activateHouseBrush();
        }
      },
      " House");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_LOCATION_PIN, selected_brush_ == 9,
      "Waypoint - place navigation waypoints (W)",
      [this]() {
        selected_brush_ = 9;
        if (controller_) {
          controller_->activateWaypointBrush();
        }
      },
      " Waypoint");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_RIGHT_FROM_BRACKET, selected_brush_ == 10,
      "House exit - set the selected house entrance tile",
      [this]() {
        selected_brush_ = 10;
        if (controller_) {
          controller_->activateHouseExitBrush();
        }
      },
      " Exit");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_BORDER_ALL, selected_brush_ == 11,
      "Optional border tool for grounds with optional overlays",
      [this]() {
        selected_brush_ = 11;
        if (controller_) {
          controller_->activateOptionalBorderBrush();
        }
      },
      " Opt");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_DOOR_OPEN, selected_brush_ == 12, "Normal door brush",
      [this]() {
        selected_brush_ = 12;
        if (controller_) {
          controller_->activateNormalDoorBrush();
        }
      },
      " Door");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_DOOR_OPEN, selected_brush_ == 18, "Normal Alt door brush",
      [this]() {
        selected_brush_ = 18;
        if (controller_) {
          controller_->activateNormalAltDoorBrush();
        }
      },
      " Alt");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_KEY, selected_brush_ == 13, "Locked door brush",
      [this]() {
        selected_brush_ = 13;
        if (controller_) {
          controller_->activateLockedDoorBrush();
        }
      },
      " Locked");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_SCROLL, selected_brush_ == 14, "Quest door brush",
      [this]() {
        selected_brush_ = 14;
        if (controller_) {
          controller_->activateQuestDoorBrush();
        }
      },
      " Quest");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_WAND_MAGIC_SPARKLES, selected_brush_ == 15, "Magic door brush",
      [this]() {
        selected_brush_ = 15;
        if (controller_) {
          controller_->activateMagicDoorBrush();
        }
      },
      " Magic");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_ARCHWAY, selected_brush_ == 16, "Archway brush",
      [this]() {
        selected_brush_ = 16;
        if (controller_) {
          controller_->activateArchwayBrush();
        }
      },
      " Arch");
  ImGui::SameLine();

  Utils::RenderToggleButton(
      ICON_FA_WINDOW_MAXIMIZE, selected_brush_ == 17, "Window brush",
      [this]() {
        selected_brush_ = 17;
        if (controller_) {
          controller_->activateWindowBrush();
        }
      },
      " Window");
  ImGui::SameLine();

  if (supportsHouseAssignment(currentBrush) && controller_) {
    uint32_t houseId = currentBrush->getType() == Brushes::BrushType::House
                           ? controller_->getHouseBrush()->getHouseId()
                           : controller_->getHouseExitBrush()->getHouseId();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputScalar("##HouseId", ImGuiDataType_U32, &houseId)) {
      controller_->getHouseBrush()->setHouseId(houseId);
      controller_->getHouseExitBrush()->setHouseId(houseId);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Active house ID");
    }
    ImGui::SameLine();
  }

  if (supportsWaypointName(currentBrush) && controller_) {
    static char waypointName[128] = "";
    auto *waypointBrush = controller_->getWaypointBrush();
    if (std::string_view(waypointName).empty() &&
        !waypointBrush->getWaypointName().empty()) {
      std::snprintf(waypointName, sizeof(waypointName), "%s",
                    waypointBrush->getWaypointName().c_str());
    }
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::InputText("##WaypointName", waypointName,
                         sizeof(waypointName))) {
      waypointBrush->setWaypointName(waypointName);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Waypoint name");
    }
    ImGui::SameLine();
  }

  if (supportsLockDoors(currentBrush) && settingsService_) {
    bool lockDoors = settingsService_->getLockDoors();
    if (ImGui::Checkbox("##LockDoors", &lockDoors)) {
      settingsService_->setLockDoors(lockDoors);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Prefer locked door variants while painting");
    }
    ImGui::SameLine();
  }

  if (supportsPreviewBorder(currentBrush) && settingsService_) {
    bool previewBorder = settingsService_->getPreviewBorder();
    if (ImGui::Checkbox("##PreviewBorder", &previewBorder)) {
      settingsService_->setPreviewBorder(previewBorder);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable outline/autoborder-style preview generation");
    }
    ImGui::SameLine();
  }

  Utils::RenderSeparator();
  ImGui::SameLine();
  renderShapeControls();
  ImGui::SameLine();
  Utils::RenderSeparator();
  ImGui::SameLine();
  renderSizeControls();
}

void BrushesPanel::renderShapeControls() {
  if (!settingsService_) {
    ImGui::TextDisabled(ICON_FA_SHAPES " Shape: N/A");
    return;
  }

  ImGui::Text(ICON_FA_SHAPES);
  ImGui::SameLine();

  const auto currentType = settingsService_->getBrushType();

  const bool isSquare = currentType == Services::BrushType::Square;
  Utils::RenderToggleButton(
      ICON_FA_VECTOR_SQUARE, isSquare, "Square brush shape",
      [this]() { settingsService_->setBrushType(Services::BrushType::Square); },
      "##Square");

  ImGui::SameLine();

  const bool isCircle = currentType == Services::BrushType::Circle;
  Utils::RenderToggleButton(
      ICON_FA_CIRCLE, isCircle, "Circle brush shape",
      [this]() { settingsService_->setBrushType(Services::BrushType::Circle); },
      "##Circle");

  ImGui::SameLine();

  const bool isCustom = currentType == Services::BrushType::Custom;
  Utils::RenderToggleButton(
      ICON_FA_PUZZLE_PIECE, isCustom, "Custom brush shape",
      [this]() { settingsService_->setBrushType(Services::BrushType::Custom); },
      "##Custom");
}

void BrushesPanel::renderSizeControls() {
  if (!settingsService_) {
    ImGui::TextDisabled(ICON_FA_CIRCLE_DOT " Size: N/A");
    return;
  }

  ImGui::Text(ICON_FA_CIRCLE_DOT " Size:");
  ImGui::SameLine();

  int brushSize = settingsService_->getStandardSize();
  const auto progression =
      Services::BrushSettingsService::getStandardSizeProgression();

  if (ImGui::Button(ICON_FA_MINUS "##BrushMinus")) {
    settingsService_->decreaseSize();
    brushSize = settingsService_->getStandardSize();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Decrease brush size (-)");
  }

  ImGui::SameLine();

  std::string currentLabel = std::format("{}x{}", brushSize, brushSize);
  ImGui::SetNextItemWidth(80.0f);
  if (ImGui::BeginCombo("##BrushSize", currentLabel.c_str())) {
    for (const auto size : progression) {
      const bool selected = size == brushSize;
      const std::string optionLabel = std::format("{}x{}", size, size);
      if (ImGui::Selectable(optionLabel.c_str(), selected)) {
        settingsService_->setStandardSize(size);
        brushSize = settingsService_->getStandardSize();
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Brush Size: %d tiles (%dx%d)", brushSize, brushSize,
                      brushSize);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_FA_PLUS "##BrushPlus")) {
    settingsService_->increaseSize();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Increase brush size (+)");
  }
}

} // namespace Ribbon
} // namespace UI
} // namespace MapEditor
