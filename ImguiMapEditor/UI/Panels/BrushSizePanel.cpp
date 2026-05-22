#include "BrushSizePanel.h"
#include "../../Utils/StringCopy.h"

#include "Brushes/BrushController.h"
#include "Services/BrushSettingsService.h"
#include "Brushes/Types/WaypointBrush.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <imgui.h>
#include <set>
#include <string>
#include <string_view>

namespace MapEditor {
namespace UI {
namespace Panels {

// Green highlight color for active toggles
static const ImVec4 ACTIVE_TOGGLE_COLOR = ImVec4(0.2f, 0.7f, 0.3f, 1.0f);

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

[[nodiscard]] bool supportsThickness(const Brushes::IBrush *brush) {
  return brush && brush->getType() == Brushes::BrushType::Doodad;
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

} // namespace

BrushSizePanel::BrushSizePanel(Services::BrushSettingsService *brushService,
                               Brushes::BrushController *brushController,
                               SaveCallback onSave)
    : service_(brushService), controller_(brushController),
      onSave_(std::move(onSave)) {
  // Initialize 11×11 custom grid
  customGrid_.resize(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
  // Set center cell as default
  customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
}

void BrushSizePanel::render(bool *p_visible) {
  if (!service_) {
    return;
  }

  if (p_visible && !*p_visible) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(ICON_FA_PAINTBRUSH " Tool Options", p_visible)) {
    renderToolbar();

    ImGui::Separator();
    renderSizeSliders();

    ImGui::Separator();
    renderBrushOptions();

    if (ImGui::CollapsingHeader(ICON_FA_SHAPES " Advanced Brush Editor")) {
      renderTopRow();
      ImGui::Separator();
      renderCustomBrushControls();
      ImGui::Separator();
      renderPreviewSection(140.0f,
                           service_->getBrushType() == Services::BrushType::Custom);
      if (service_->getBrushType() == Services::BrushType::Custom) {
        ImGui::Separator();
        renderBottomButtons();
      }
    }
  }
  ImGui::End();
}

void BrushSizePanel::renderToolbar() {
  if (!controller_) {
    ImGui::TextDisabled("Brush shortcuts unavailable");
    return;
  }

  const auto *currentBrush = controller_->getCurrentBrush();
  if (currentBrush) {
    ImGui::TextDisabled("Active: %s", currentBrush->getName().c_str());
  } else {
    ImGui::TextDisabled("Active: Selection");
  }

  auto renderBrushButton = [&](const char *id, const char *icon,
                               [[maybe_unused]] const char *tooltip, bool active,
                               const std::function<void()> &onClick) {
    ImGui::PushID(id);
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_TOGGLE_COLOR);
    }
    if (ImGui::Button(icon, ImVec2(26.0f, 0.0f))) {
      onClick();
    }
    if (active) {
      ImGui::PopStyleColor();
    }
    ImGui::PopID();
  };

  const auto *spawnBrush = controller_->getSpawnBrush();
  const auto *pzBrush = controller_->getPZBrush();
  const auto *noPvpBrush = controller_->getNoPvpBrush();
  const auto *noLogoutBrush = controller_->getNoLogoutBrush();
  const auto *pvpBrush = controller_->getPvpZoneBrush();
  const auto *eraserBrush = controller_->getEraserBrush();
  const auto *houseBrush = controller_->getHouseBrush();
  const auto *houseExitBrush = controller_->getHouseExitBrush();
  const auto *waypointBrush = controller_->getWaypointBrush();
  const auto *optionalBorderBrush = controller_->getOptionalBorderBrush();
  const auto *normalDoorBrush = controller_->getNormalDoorBrush();
  const auto *normalAltDoorBrush = controller_->getNormalAltDoorBrush();
  const auto *lockedDoorBrush = controller_->getLockedDoorBrush();
  const auto *questDoorBrush = controller_->getQuestDoorBrush();
  const auto *magicDoorBrush = controller_->getMagicDoorBrush();
  const auto *archwayBrush = controller_->getArchwayBrush();
  const auto *windowBrush = controller_->getWindowBrush();
  const auto *hatchWindowBrush = controller_->getHatchWindowBrush();

  auto hasSame = [&](const Brushes::IBrush *brush) {
    return brush && controller_->isCurrentBrush(brush);
  };

  auto hasSameEither = [&](const Brushes::IBrush *brush,
                           const Brushes::IBrush *alternate) {
    return hasSame(brush) || hasSame(alternate);
  };

  renderBrushButton("spawn", ICON_FA_LOCATION_DOT, "Spawn brush",
                    hasSame(spawnBrush),
                    [this]() { controller_->activateSpawnBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("pz", ICON_FA_SHIELD_HALVED, "Protection Zone brush",
                    hasSame(pzBrush), [this]() { controller_->activatePZBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("nopvp", ICON_FA_HAND, "No PvP brush", hasSame(noPvpBrush),
                    [this]() { controller_->activateNoPvpBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("nologout", ICON_FA_DOOR_CLOSED, "No Logout brush",
                    hasSame(noLogoutBrush),
                    [this]() { controller_->activateNoLogoutBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("pvp", ICON_FA_SKULL, "PvP Zone brush", hasSame(pvpBrush),
                    [this]() { controller_->activatePvpZoneBrush(); });

  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("eraser", ICON_FA_ERASER, "Eraser brush",
                    hasSame(eraserBrush),
                    [this]() { controller_->activateEraserBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("house", ICON_FA_HOUSE, "House brush", hasSame(houseBrush),
                    [this]() { controller_->activateHouseBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("house_exit", ICON_FA_RIGHT_FROM_BRACKET,
                    "House exit brush", hasSame(houseExitBrush),
                    [this]() { controller_->activateHouseExitBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("waypoint", ICON_FA_LOCATION_PIN, "Waypoint brush",
                    hasSame(waypointBrush),
                    [this]() { controller_->activateWaypointBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("opt_border", ICON_FA_BORDER_ALL, "Optional border brush",
                    hasSame(optionalBorderBrush),
                    [this]() { controller_->activateOptionalBorderBrush(); });

  ImGui::NewLine();
  renderBrushButton("door_normal", ICON_FA_DOOR_OPEN, "Normal door brush",
                    hasSameEither(normalDoorBrush, normalAltDoorBrush),
                    [this]() { controller_->activateNormalDoorBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_locked", ICON_FA_KEY, "Locked door brush",
                    hasSame(lockedDoorBrush),
                    [this]() { controller_->activateLockedDoorBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_quest", ICON_FA_SCROLL, "Quest door brush",
                    hasSame(questDoorBrush),
                    [this]() { controller_->activateQuestDoorBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_magic", ICON_FA_WAND_MAGIC_SPARKLES,
                    "Magic door brush", hasSame(magicDoorBrush),
                    [this]() { controller_->activateMagicDoorBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_arch", ICON_FA_ARCHWAY, "Archway brush",
                    hasSame(archwayBrush),
                    [this]() { controller_->activateArchwayBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_window", ICON_FA_WINDOW_MAXIMIZE, "Window brush",
                    hasSame(windowBrush),
                    [this]() { controller_->activateWindowBrush(); });
  ImGui::SameLine(0.0f, 4.0f);
  renderBrushButton("door_hatch_window", ICON_FA_WINDOW_MAXIMIZE,
                    "Hatch window brush", hasSame(hatchWindowBrush),
                    [this]() { controller_->activateHatchWindowBrush(); });
}

void BrushSizePanel::renderBrushOptions() {
  const auto *currentBrush = controller_ ? controller_->getCurrentBrush() : nullptr;
  if (!currentBrush) {
    ImGui::TextDisabled("No active brush");
    return;
  }

  bool showedSection = false;

  const auto beginSection = [&]() {
    if (showedSection) {
      ImGui::Separator();
    }
    showedSection = true;
  };

  if (supportsSpawnSettings(currentBrush)) {
    beginSection();
    renderSpawnSection();
  }

  if (supportsHouseAssignment(currentBrush)) {
    beginSection();
    uint32_t houseId = currentBrush->getType() == Brushes::BrushType::House
                           ? controller_->getHouseBrush()->getHouseId()
                           : controller_->getHouseExitBrush()->getHouseId();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputScalar("House ID", ImGuiDataType_U32, &houseId)) {
      controller_->getHouseBrush()->setHouseId(houseId);
      controller_->getHouseExitBrush()->setHouseId(houseId);
    }
    ImGui::TextDisabled("Shared house assignment for House and House Exit.");
  }

  if (supportsWaypointName(currentBrush)) {
    beginSection();
    auto *waypointBrush = controller_->getWaypointBrush();
    if (waypointBrush && cachedWaypointBrush_ != waypointBrush) {
      cachedWaypointBrush_ = waypointBrush;
      ::MapEditor::Utils::copyTruncate(waypointNameBuffer_,
                                       waypointBrush->getWaypointName());
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("Waypoint Name", waypointNameBuffer_,
                         static_cast<int>(sizeof(waypointNameBuffer_))) &&
        waypointBrush) {
      waypointBrush->setWaypointName(waypointNameBuffer_);
    }
    ImGui::TextDisabled("Used by selection and saving.");
  }

  if (supportsThickness(currentBrush)) {
    beginSection();
    float thickness = controller_->getBrushThickness();
    ImGui::TextUnformatted("Thickness");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##Thickness", &thickness, 0.0f, 1.0f, "%.2f")) {
      controller_->setBrushThickness(thickness);
    }
    ImGui::TextDisabled("Doodad placement weight.");
  }

  if (supportsLockDoors(currentBrush)) {
    beginSection();
    bool lockDoors = service_->getLockDoors();
    if (ImGui::Checkbox("Lock Doors", &lockDoors)) {
      service_->setLockDoors(lockDoors);
    }
    ImGui::TextDisabled("Applies locked preference while painting doors.");
  }

  if (supportsPreviewBorder(currentBrush)) {
    beginSection();
    bool previewBorder = service_->getPreviewBorder();
    if (ImGui::Checkbox("Preview Border", &previewBorder)) {
      service_->setPreviewBorder(previewBorder);
    }
    ImGui::TextDisabled("Controls outline/autoborder-style preview generation.");
  }

  if (!showedSection) {
    ImGui::TextDisabled("No brush-specific options");
  }
}

void BrushSizePanel::renderTopRow() {
  auto currentType = service_->getBrushType();

  auto renderShapeBtn = [&](const char *icon, Services::BrushType type,
                            [[maybe_unused]] const char *tooltip) {
    bool isSelected = (currentType == type);
    if (isSelected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_TOGGLE_COLOR);
    }
    if (ImGui::Button(icon)) {
      service_->setBrushType(type);
      // Load selected brush to grid when switching to Custom
      if (type == Services::BrushType::Custom) {
        loadSelectedBrushToGrid();
      }
    }
    if (isSelected) {
      ImGui::PopStyleColor();
    }
    ImGui::SameLine();
  };

  renderShapeBtn(ICON_FA_SQUARE, Services::BrushType::Square, "Square brush");
  renderShapeBtn(ICON_FA_CIRCLE, Services::BrushType::Circle, "Circle brush");
  renderShapeBtn(ICON_FA_PUZZLE_PIECE, Services::BrushType::Custom,
                 "Custom brush shape");

  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();

  if (symmetricSize_) {
    ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_TOGGLE_COLOR);
  }
  if (ImGui::Button(symmetricSize_ ? ICON_FA_LINK : ICON_FA_LINK_SLASH)) {
    symmetricSize_ = !symmetricSize_;
    if (symmetricSize_) {
      int w = service_->getCustomWidth();
      service_->setCustomDimensions(w, w);
    }
  }
  if (symmetricSize_) {
    ImGui::PopStyleColor();
  }
}

void BrushSizePanel::renderSizeSliders() {
  if (service_->getBrushSizeMode() != Services::BrushSizeMode::Standard) {
    service_->setBrushSizeMode(Services::BrushSizeMode::Standard);
  }

  const auto discreteSizes =
      Services::BrushSettingsService::getStandardSizeProgression();
  int size = service_->getStandardSize();
  ImGui::TextUnformatted("Brush Size");
  ImGui::SameLine();
  ImGui::TextDisabled("(discrete, Alt + wheel)");

  if (ImGui::Button(ICON_FA_MINUS "##BrushSizeDown")) {
    service_->decreaseSize();
    size = service_->getStandardSize();
  }
  ImGui::SameLine(0.0f, 4.0f);

  std::string sizeLabel = std::to_string(size);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 34.0f);
  if (ImGui::BeginCombo("##BrushSizeDiscrete", sizeLabel.c_str())) {
    for (const auto discreteSize : discreteSizes) {
      const bool selected = size == discreteSize;
      const std::string optionLabel = std::format("{}x{}", discreteSize,
                                                  discreteSize);
      if (ImGui::Selectable(optionLabel.c_str(), selected)) {
        service_->setStandardSize(discreteSize);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine(0.0f, 4.0f);

  if (ImGui::Button(ICON_FA_PLUS "##BrushSizeUp")) {
    service_->increaseSize();
  }
  ImGui::TextDisabled("wx-style size steps are shared by the active brush family.");
}

void BrushSizePanel::renderCustomBrushControls() {
  const auto &brushes = service_->getCustomBrushes();
  const auto *selected = service_->getSelectedCustomBrush();

  // Dropdown for brush selection (full width)
  const char *currentName = selected ? selected->name.c_str() : "Default";

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::BeginCombo("##BrushSelect", currentName)) {
    // Default option (single center tile)
    bool isDefault = (selected == nullptr);
    if (ImGui::Selectable("Default", isDefault)) {
      service_->selectCustomBrush(""); // Empty = default
      // Reset grid to single center
      for (auto &row : customGrid_) {
        std::fill(row.begin(), row.end(), false);
      }
      customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
      editingBrushName_ = "Default";
      isNewBrushMode_ = false;
      syncGridToService();
    }

    // Saved brushes
    int brushIndex = 0;
    for (const auto &brush : brushes) {
      bool isSelected = (selected && selected->name == brush.name);
      // Use index suffix for unique ID in case of duplicate names
      std::string label = brush.name + "##brush" + std::to_string(brushIndex++);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
        service_->selectCustomBrush(brush.name);
        loadSelectedBrushToGrid();
        editingBrushName_ = brush.name; // Sync name with selection
        isNewBrushMode_ = false;
      }
    }
    ImGui::EndCombo();
  }

  // Brush name input (always visible)
  char nameBuffer[64];
  ::MapEditor::Utils::copyTruncate(nameBuffer, editingBrushName_);

  // Pulsing green border when in "new" mode
  if (isNewBrushMode_) {
    float pulse = 0.5f + 0.5f * std::sin(ImGui::GetTime() * 4.0f);
    ImVec4 pulseColor = ImVec4(0.2f, 0.7f * pulse + 0.3f, 0.3f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, pulseColor);
  }

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##BrushName", nameBuffer, sizeof(nameBuffer))) {
    editingBrushName_ = nameBuffer;
  }

  if (isNewBrushMode_) {
    ImGui::PopStyleColor();
  }
}

void BrushSizePanel::renderPreviewSection(float availableHeight,
                                          bool isInteractive) {
  if (ImGui::CollapsingHeader(ICON_FA_EYE " Preview",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    float maxSize =
        std::min(ImGui::GetContentRegionAvail().x, availableHeight - 20.0f);

    if (isInteractive) {
      drawInteractiveGrid(maxSize);
    } else {
      drawReadOnlyGrid(maxSize);
    }
  }
}

void BrushSizePanel::renderBottomButtons() {
  float buttonWidth = (ImGui::GetContentRegionAvail().x - 12) / 4;
  const auto *selected = service_->getSelectedCustomBrush();

  // New button
  if (ImGui::Button(ICON_FA_PLUS "##New", ImVec2(buttonWidth, 0))) {
    // Clear grid
    for (auto &row : customGrid_) {
      std::fill(row.begin(), row.end(), false);
    }
    customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
    syncGridToService();
    // Set placeholder and start new mode
    editingBrushName_ = "Enter shape name";
    isNewBrushMode_ = true;
    service_->selectCustomBrush(""); // Deselect current
  }

  ImGui::SameLine();

  // Save button
  if (ImGui::Button(ICON_FA_FLOPPY_DISK "##Save", ImVec2(buttonWidth, 0))) {
    if (!editingBrushName_.empty() && editingBrushName_ != "Enter shape name") {
      saveGridAsNewBrush();
      isNewBrushMode_ = false;
    }
  }

  ImGui::SameLine();

  // Clear button
  if (ImGui::Button(ICON_FA_ERASER "##Clear", ImVec2(buttonWidth, 0))) {
    for (auto &row : customGrid_) {
      std::fill(row.begin(), row.end(), false);
    }
    customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
    syncGridToService();
  }

  ImGui::SameLine();

  // Delete button
  ImGui::BeginDisabled(selected == nullptr);
  if (ImGui::Button(ICON_FA_TRASH "##Delete", ImVec2(buttonWidth, 0))) {
    deleteCurrentBrush();
  }
  ImGui::EndDisabled();
}

void BrushSizePanel::drawInteractiveGrid(float maxSize) {
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float cellSize = std::max(8.0f, maxSize / GRID_SIZE);
  cellSize = std::min(cellSize, 18.0f);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 cursor = ImGui::GetCursorScreenPos();

  float totalWidth = GRID_SIZE * cellSize;
  float totalHeight = GRID_SIZE * cellSize;
  float offsetX = (avail.x - totalWidth) / 2.0f;

  ImVec2 gridMin(cursor.x + offsetX, cursor.y);
  drawList->AddRectFilled(
      gridMin, ImVec2(gridMin.x + totalWidth, gridMin.y + totalHeight),
      IM_COL32(40, 40, 40, 255));

  bool mouseDown = ImGui::IsMouseDown(0);
  ImVec2 mousePos = ImGui::GetMousePos();
  bool gridChanged = false;
  int center = GRID_SIZE / 2;

  for (int y = 0; y < GRID_SIZE; ++y) {
    for (int x = 0; x < GRID_SIZE; ++x) {
      ImVec2 cellMin(gridMin.x + x * cellSize, gridMin.y + y * cellSize);
      ImVec2 cellMax(cellMin.x + cellSize - 1, cellMin.y + cellSize - 1);

      bool hovered = (mousePos.x >= cellMin.x && mousePos.x < cellMax.x &&
                      mousePos.y >= cellMin.y && mousePos.y < cellMax.y);

      // Handle click
      if (hovered && mouseDown) {
        if (ImGui::IsMouseClicked(0)) {
          customGrid_[y][x] = !customGrid_[y][x];
          gridChanged = true;
        } else if (ImGui::GetIO().KeyCtrl) {
          // Drag paint
          if (!customGrid_[y][x]) {
            customGrid_[y][x] = true;
            gridChanged = true;
          }
        }
      }

      // Draw cell
      if (customGrid_[y][x]) {
        drawList->AddRectFilled(cellMin, cellMax, IM_COL32(100, 180, 255, 255));
      } else if (hovered) {
        drawList->AddRectFilled(cellMin, cellMax, IM_COL32(70, 70, 70, 255));
      }

      drawList->AddRect(cellMin, cellMax, IM_COL32(60, 60, 60, 255));

      // Mark center
      if (x == center && y == center) {
        drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 0, 255), 0, 0,
                          2.0f);
      }
    }
  }

  ImGui::Dummy(ImVec2(totalWidth, totalHeight + 4));

  // Sync changes to service
  if (gridChanged) {
    syncGridToService();
  }
}

void BrushSizePanel::drawReadOnlyGrid(float maxSize) {
  auto offsets = service_->getBrushOffsets();

  ImVec2 avail = ImGui::GetContentRegionAvail();
  float cellSize = std::max(8.0f, maxSize / GRID_SIZE);
  cellSize = std::min(cellSize, 18.0f);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 cursor = ImGui::GetCursorScreenPos();

  float totalWidth = GRID_SIZE * cellSize;
  float totalHeight = GRID_SIZE * cellSize;
  float offsetX = (avail.x - totalWidth) / 2.0f;

  ImVec2 gridMin(cursor.x + offsetX, cursor.y);
  drawList->AddRectFilled(
      gridMin, ImVec2(gridMin.x + totalWidth, gridMin.y + totalHeight),
      IM_COL32(40, 40, 40, 255));

  std::set<std::pair<int, int>> offsetSet(offsets.begin(), offsets.end());
  int center = GRID_SIZE / 2;

  for (int y = 0; y < GRID_SIZE; ++y) {
    for (int x = 0; x < GRID_SIZE; ++x) {
      int dx = x - center;
      int dy = y - center;

      ImVec2 cellMin(gridMin.x + x * cellSize, gridMin.y + y * cellSize);
      ImVec2 cellMax(cellMin.x + cellSize - 1, cellMin.y + cellSize - 1);

      if (offsetSet.count({dx, dy})) {
        drawList->AddRectFilled(cellMin, cellMax, IM_COL32(100, 180, 255, 255));
      }

      drawList->AddRect(cellMin, cellMax, IM_COL32(60, 60, 60, 255));

      if (dx == 0 && dy == 0) {
        drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 0, 255), 0, 0,
                          2.0f);
      }
    }
  }

  ImGui::Dummy(ImVec2(totalWidth, totalHeight + 4));
}

void BrushSizePanel::renderPresetButtons() {
  float buttonWidth = (ImGui::GetContentRegionAvail().x - 12) / 4;

  if (ImGui::Button("Clear", ImVec2(buttonWidth, 0))) {
    applyPreset("clear");
  }
  ImGui::SameLine();
  if (ImGui::Button("3x3", ImVec2(buttonWidth, 0))) {
    applyPreset("3x3");
  }
  ImGui::SameLine();
  if (ImGui::Button("5x5", ImVec2(buttonWidth, 0))) {
    applyPreset("5x5");
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_DIAMOND, ImVec2(buttonWidth, 0))) {
    applyPreset("diamond");
  }
}

void BrushSizePanel::loadSelectedBrushToGrid() {
  const auto *brush = service_->getSelectedCustomBrush();

  // Clear grid
  for (auto &row : customGrid_) {
    std::fill(row.begin(), row.end(), false);
  }

  if (brush) {
    // Load offsets to grid
    int center = GRID_SIZE / 2;
    for (const auto &[dx, dy] : brush->offsets) {
      int gx = center + dx;
      int gy = center + dy;
      if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
        customGrid_[gy][gx] = true;
      }
    }
  } else {
    // Default: single center tile
    customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
  }
}

void BrushSizePanel::saveGridAsNewBrush() {
  if (editingBrushName_.empty()) {
    return;
  }

  Services::CustomBrushShape brush(editingBrushName_, GRID_SIZE);
  brush.grid = customGrid_;
  brush.computeOffsets();

  if (!brush.isEmpty()) {
    service_->addCustomBrush(brush);
    service_->selectCustomBrush(editingBrushName_);
    autoSaveBrushes();
  }
}

void BrushSizePanel::saveGridToCurrentBrush() {
  const auto *current = service_->getSelectedCustomBrush();
  if (!current) {
    return;
  }

  Services::CustomBrushShape brush(current->name, GRID_SIZE);
  brush.grid = customGrid_;
  brush.computeOffsets();

  if (!brush.isEmpty()) {
    service_->addCustomBrush(brush); // Will overwrite existing
    autoSaveBrushes();
  }
}

void BrushSizePanel::deleteCurrentBrush() {
  const auto *current = service_->getSelectedCustomBrush();
  if (!current) {
    return;
  }

  std::string nameToDelete = current->name;
  service_->removeCustomBrush(nameToDelete);
  loadSelectedBrushToGrid(); // Reset to default
  autoSaveBrushes();
}

void BrushSizePanel::applyPreset(const char *preset) {
  const std::string_view presetName = preset ? preset : "";

  // Clear grid
  for (auto &row : customGrid_) {
    std::fill(row.begin(), row.end(), false);
  }

  int center = GRID_SIZE / 2;

  if (presetName == "clear") {
    customGrid_[center][center] = true; // Always at least one
  } else if (presetName == "3x3") {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        customGrid_[center + dy][center + dx] = true;
      }
    }
  } else if (presetName == "5x5") {
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        customGrid_[center + dy][center + dx] = true;
      }
    }
  } else if (presetName == "diamond") {
    // Diamond pattern
    customGrid_[center][center] = true;
    customGrid_[center - 1][center] = true;
    customGrid_[center + 1][center] = true;
    customGrid_[center][center - 1] = true;
    customGrid_[center][center + 1] = true;
    customGrid_[center - 2][center] = true;
    customGrid_[center + 2][center] = true;
    customGrid_[center][center - 2] = true;
    customGrid_[center][center + 2] = true;
    customGrid_[center - 1][center - 1] = true;
    customGrid_[center - 1][center + 1] = true;
    customGrid_[center + 1][center - 1] = true;
    customGrid_[center + 1][center + 1] = true;
  }

  syncGridToService();
}

void BrushSizePanel::syncGridToService() {
  // Create temporary brush to update offsets
  Services::CustomBrushShape tempBrush("", GRID_SIZE);
  tempBrush.grid = customGrid_;
  tempBrush.computeOffsets();

  // If we have a selected brush, update it
  const auto *current = service_->getSelectedCustomBrush();
  if (current) {
    Services::CustomBrushShape updated(current->name, GRID_SIZE);
    updated.grid = customGrid_;
    updated.computeOffsets();
    service_->addCustomBrush(updated);
  }
  // The service will pick up changes via callback or on next getBrushOffsets()
}

void BrushSizePanel::autoSaveBrushes() {
  if (onSave_) {
    onSave_();
  }
}

void BrushSizePanel::renderSpawnSection() {
  ImGui::TextUnformatted("Spawn Settings");
  bool autoSpawn = service_->getAutoCreateSpawn();
  if (ImGui::Checkbox("Auto-create spawn", &autoSpawn)) {
    service_->setAutoCreateSpawn(autoSpawn);
  }
  ImGui::TextDisabled("Creature brushes can create a spawn automatically.");

  if (autoSpawn) {
    ImGui::Indent(10.0f);

    const int radius = service_->getStandardSize();
    ImGui::Text("Spawn Radius: %d", radius);
    ImGui::TextDisabled("Follows current brush size for wx parity.");

    int time = service_->getDefaultSpawnTime();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputInt("Spawn Time", &time, 10, 60)) {
      service_->setDefaultSpawnTime(std::clamp(time, 1, 86400));
    }

    ImGui::Unindent(10.0f);
  }
}

} // namespace Panels
} // namespace UI
} // namespace MapEditor
