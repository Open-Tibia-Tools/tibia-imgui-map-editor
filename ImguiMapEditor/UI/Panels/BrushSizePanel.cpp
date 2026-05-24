#include "BrushSizePanel.h"

#include "Services/BrushSettingsService.h"
#include "UI/Utils/UIUtils.hpp"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <imgui.h>
#include <set>

namespace MapEditor {
namespace UI {
namespace Panels {

// Green highlight color for active toggles
static const ImVec4 ACTIVE_TOGGLE_COLOR = ImVec4(0.2f, 0.7f, 0.3f, 1.0f);

BrushSizePanel::BrushSizePanel(Services::BrushSettingsService *brushService,
                               SaveCallback onSave)
    : service_(brushService), onSave_(std::move(onSave)) {
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

  ImGui::SetNextWindowSize(ImVec2(200, 320), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(ICON_FA_PAINTBRUSH " Brush Settings", p_visible)) {
    const bool isCustomMode =
        (service_->getBrushType() == Services::BrushType::Custom);

    // Calculate layout
    const float topRowHeight = 30.0f;
    const float controlsHeight = isCustomMode ? 55.0f : 50.0f;
    const float bottomButtonsHeight = isCustomMode ? 30.0f : 0.0f;
    const float headerHeight = 24.0f;
    const float separatorHeight = 8.0f * (isCustomMode ? 4 : 3);

    const float totalFixed = topRowHeight + controlsHeight + bottomButtonsHeight +
                       headerHeight + separatorHeight;
    const float availableForPreview = ImGui::GetContentRegionAvail().y - totalFixed;

    // Top row: Shape buttons + separator + symmetric toggle
    renderTopRow();
    ImGui::Separator();

    // Size sliders OR custom brush controls
    if (isCustomMode) {
      renderCustomBrushControls();
    } else {
      renderSizeSliders();
    }
    ImGui::Separator();

    // Preview section (interactive in Custom mode)
    renderPreviewSection(std::max(80.0f, availableForPreview), isCustomMode);

    // Bottom buttons (only in Custom mode)
    if (isCustomMode) {
      ImGui::Separator();
      renderBottomButtons();
    }
  }
  ImGui::End();
}

void BrushSizePanel::renderTopRow() {
  auto currentType = service_->getBrushType();

  auto renderShapeBtn = [&](const char *icon, Services::BrushType type,
                            const char *tooltip) {
    const bool isSelected = (currentType == type);
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
    Utils::SetTooltipOnHover(tooltip);
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
      const int w = service_->getCustomWidth();
      service_->setCustomDimensions(w, w);
    }
  }
  if (symmetricSize_) {
    ImGui::PopStyleColor();
  }
  Utils::SetTooltipOnHover(
      symmetricSize_ ? "Symmetric: W=H linked (click to unlock)"
                     : "Asymmetric: W and H independent (click to link)");
}

void BrushSizePanel::renderSizeSliders() {
  service_->setBrushSizeMode(Services::BrushSizeMode::CustomDimensions);

  int width = service_->getCustomWidth();
  int height = service_->getCustomHeight();

  // Width row
  ImGui::Text(ICON_FA_ARROWS_LEFT_RIGHT);
  Utils::SetTooltipOnHover("Width");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_MINUS "##W")) {
    width = std::max(1, width - 1);
    if (symmetricSize_)
      height = width;
    service_->setCustomDimensions(width, height);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28);
  if (ImGui::SliderInt("##W", &width, Services::BrushSettingsService::MIN_SIZE,
                       Services::BrushSettingsService::MAX_SIZE, "%d")) {
    if (symmetricSize_)
      height = width;
    service_->setCustomDimensions(width, height);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_PLUS "##W")) {
    width = std::min(10, width + 1);
    if (symmetricSize_)
      height = width;
    service_->setCustomDimensions(width, height);
  }

  // Height row
  ImGui::Text(ICON_FA_ARROWS_UP_DOWN);
  Utils::SetTooltipOnHover("Height");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_MINUS "##H")) {
    height = std::max(1, height - 1);
    if (symmetricSize_)
      width = height;
    service_->setCustomDimensions(width, height);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28);
  if (ImGui::SliderInt("##H", &height, Services::BrushSettingsService::MIN_SIZE,
                       Services::BrushSettingsService::MAX_SIZE, "%d")) {
    if (symmetricSize_)
      width = height;
    service_->setCustomDimensions(width, height);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_PLUS "##H")) {
    height = std::min(10, height + 1);
    if (symmetricSize_)
      width = height;
    service_->setCustomDimensions(width, height);
  }
}

void BrushSizePanel::renderCustomBrushControls() {
  const auto &brushes = service_->getCustomBrushes();
  const auto *selected = service_->getSelectedCustomBrush();

  // Dropdown for brush selection (full width)
  const char *currentName = selected ? selected->name.c_str() : "Default";

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::BeginCombo("##BrushSelect", currentName)) {
    // Default option (single center tile)
    const bool isDefault = (selected == nullptr);
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
      const bool isSelected = (selected && selected->name == brush.name);
      // Use index suffix for unique ID in case of duplicate names
      const std::string label = brush.name + "##brush" + std::to_string(brushIndex++);
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
  strncpy(nameBuffer, editingBrushName_.c_str(), sizeof(nameBuffer) - 1);
  nameBuffer[sizeof(nameBuffer) - 1] = '\0';

  // Pulsing green border when in "new" mode
  if (isNewBrushMode_) {
    const float pulse = 0.5f + 0.5f * std::sin(ImGui::GetTime() * 4.0f);
    const ImVec4 pulseColor = ImVec4(0.2f, 0.7f * pulse + 0.3f, 0.3f, 1.0f);
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
    const float maxSize =
        std::min(ImGui::GetContentRegionAvail().x, availableHeight - 20.0f);

    if (isInteractive) {
      drawInteractiveGrid(maxSize);
    } else {
      drawReadOnlyGrid(maxSize);
    }
  }
}

void BrushSizePanel::renderBottomButtons() {
  const float buttonWidth = (ImGui::GetContentRegionAvail().x - 12) / 4;
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
  Utils::SetTooltipOnHover("New brush");

  ImGui::SameLine();

  // Save button
  if (ImGui::Button(ICON_FA_FLOPPY_DISK "##Save", ImVec2(buttonWidth, 0))) {
    if (!editingBrushName_.empty() && editingBrushName_ != "Enter shape name") {
      saveGridAsNewBrush();
      isNewBrushMode_ = false;
    }
  }
  Utils::SetTooltipOnHover("Save brush");

  ImGui::SameLine();

  // Clear button
  if (ImGui::Button(ICON_FA_ERASER "##Clear", ImVec2(buttonWidth, 0))) {
    for (auto &row : customGrid_) {
      std::fill(row.begin(), row.end(), false);
    }
    customGrid_[GRID_SIZE / 2][GRID_SIZE / 2] = true;
    syncGridToService();
  }
  Utils::SetTooltipOnHover("Clear grid");

  ImGui::SameLine();

  // Delete button
  ImGui::BeginDisabled(selected == nullptr);
  if (ImGui::Button(ICON_FA_TRASH "##Delete", ImVec2(buttonWidth, 0))) {
    deleteCurrentBrush();
  }
  Utils::SetTooltipOnHover("Delete brush");
  ImGui::EndDisabled();
}

void BrushSizePanel::drawInteractiveGrid(float maxSize) {
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float cellSize = std::max(8.0f, maxSize / GRID_SIZE);
  cellSize = std::min(cellSize, 18.0f);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const ImVec2 cursor = ImGui::GetCursorScreenPos();

  const float totalWidth = GRID_SIZE * cellSize;
  const float totalHeight = GRID_SIZE * cellSize;
  const float offsetX = (avail.x - totalWidth) / 2.0f;

  const ImVec2 gridMin(cursor.x + offsetX, cursor.y);
  drawList->AddRectFilled(
      gridMin, ImVec2(gridMin.x + totalWidth, gridMin.y + totalHeight),
      IM_COL32(40, 40, 40, 255));

  const bool mouseDown = ImGui::IsMouseDown(0);
  const ImVec2 mousePos = ImGui::GetMousePos();
  bool gridChanged = false;
  const int center = GRID_SIZE / 2;

  for (int y = 0; y < GRID_SIZE; ++y) {
    for (int x = 0; x < GRID_SIZE; ++x) {
      const ImVec2 cellMin(gridMin.x + x * cellSize, gridMin.y + y * cellSize);
      const ImVec2 cellMax(cellMin.x + cellSize - 1, cellMin.y + cellSize - 1);

      const bool hovered = (mousePos.x >= cellMin.x && mousePos.x < cellMax.x &&
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
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  float cellSize = std::max(8.0f, maxSize / GRID_SIZE);
  cellSize = std::min(cellSize, 18.0f);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const ImVec2 cursor = ImGui::GetCursorScreenPos();

  const float totalWidth = GRID_SIZE * cellSize;
  const float totalHeight = GRID_SIZE * cellSize;
  const float offsetX = (avail.x - totalWidth) / 2.0f;

  const ImVec2 gridMin(cursor.x + offsetX, cursor.y);
  drawList->AddRectFilled(
      gridMin, ImVec2(gridMin.x + totalWidth, gridMin.y + totalHeight),
      IM_COL32(40, 40, 40, 255));

  const std::set<std::pair<int, int>> offsetSet(offsets.begin(), offsets.end());
  const int center = GRID_SIZE / 2;

  for (int y = 0; y < GRID_SIZE; ++y) {
    for (int x = 0; x < GRID_SIZE; ++x) {
      const int dx = x - center;
      const int dy = y - center;

      const ImVec2 cellMin(gridMin.x + x * cellSize, gridMin.y + y * cellSize);
      const ImVec2 cellMax(cellMin.x + cellSize - 1, cellMin.y + cellSize - 1);

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
  const float buttonWidth = (ImGui::GetContentRegionAvail().x - 12) / 4;

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
  Utils::SetTooltipOnHover("Diamond shape");
}

void BrushSizePanel::loadSelectedBrushToGrid() {
  const auto *brush = service_->getSelectedCustomBrush();

  // Clear grid
  for (auto &row : customGrid_) {
    std::fill(row.begin(), row.end(), false);
  }

  if (brush) {
    // Load offsets to grid
    const int center = GRID_SIZE / 2;
    for (const auto &[dx, dy] : brush->offsets) {
      const int gx = center + dx;
      const int gy = center + dy;
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

  const std::string nameToDelete = current->name;
  service_->removeCustomBrush(nameToDelete);
  loadSelectedBrushToGrid(); // Reset to default
  autoSaveBrushes();
}

void BrushSizePanel::applyPreset(const char *preset) {
  // Clear grid
  for (auto &row : customGrid_) {
    std::fill(row.begin(), row.end(), false);
  }

  const int center = GRID_SIZE / 2;

  if (strcmp(preset, "clear") == 0) {
    customGrid_[center][center] = true; // Always at least one
  } else if (strcmp(preset, "3x3") == 0) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        customGrid_[center + dy][center + dx] = true;
      }
    }
  } else if (strcmp(preset, "5x5") == 0) {
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        customGrid_[center + dy][center + dx] = true;
      }
    }
  } else if (strcmp(preset, "diamond") == 0) {
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
  if (ImGui::CollapsingHeader(ICON_FA_LOCATION_DOT " Spawn Settings",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    // Auto-create spawn checkbox
    bool autoSpawn = service_->getAutoCreateSpawn();
    if (ImGui::Checkbox("Auto-create spawn", &autoSpawn)) {
      service_->setAutoCreateSpawn(autoSpawn);
    }
    Utils::SetTooltipOnHover(
        "When placing creatures, automatically create a spawn point");

    // Only show radius/timer if auto-spawn is enabled OR it's useful context
    if (autoSpawn) {
      ImGui::Indent(10.0f);

      // Spawn radius slider
      int radius = service_->getDefaultSpawnRadius();
      ImGui::Text(ICON_FA_CIRCLE_NOTCH);
      Utils::SetTooltipOnHover("Spawn radius (tiles)");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (ImGui::SliderInt("##SpawnRadius", &radius, 1, 10, "Radius: %d")) {
        service_->setDefaultSpawnRadius(radius);
      }

      // Spawn timer input
      int time = service_->getDefaultSpawnTime();
      ImGui::Text(ICON_FA_CLOCK);
      Utils::SetTooltipOnHover("Spawn timer (seconds)");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (ImGui::InputInt("##SpawnTime", &time, 10, 60)) {
        service_->setDefaultSpawnTime(std::clamp(time, 1, 86400));
      }

      ImGui::Unindent(10.0f);
    }
  }
}

} // namespace Panels
} // namespace UI
} // namespace MapEditor
