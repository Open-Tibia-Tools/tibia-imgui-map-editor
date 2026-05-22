#include "TilesetWidget.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <string_view>

#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "../../Brushes/BrushController.h"
#include "../../Brushes/Types/RawBrush.h"
#include "../../Domain/Tileset/TilesetEntry.h"
#include "../../Services/ClientDataService.h"
#include "../../Services/SpriteManager.h"
#include "../Utils/BrushPreviewRenderer.h"
#include "../Utils/BrushPreviewResolver.h"
#include "../Utils/UIUtils.hpp"

namespace MapEditor::UI {

using namespace Domain::Tileset;

constexpr float FILTER_INPUT_WIDTH = 150.0f;

TilesetWidget::TilesetWidget() = default;

TilesetWidget::~TilesetWidget() = default;

void TilesetWidget::initialize(
    Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager,
    Brushes::BrushController *brushController,
    Domain::Tileset::TilesetRegistry &tilesetRegistry) {
  clientData_ = clientData;
  spriteManager_ = spriteManager;
  brushController_ = brushController;
  tilesetRegistry_ = &tilesetRegistry;
}

void TilesetWidget::setIconSize(float size) {
  iconSize_ = std::clamp(size, 32.0f, 128.0f);
}

void TilesetWidget::render(bool *p_visible) {
  if (!ImGui::Begin("Palettes", p_visible)) {
    ImGui::End();
    return;
  }

  syncActiveBrushSelection();

  // Row 1: Tileset dropdown (flat list of all tilesets)
  renderTilesetDropdown();

  // Row 2: Icon size slider
  renderIconSizeSlider();

  ImGui::Separator();

  // Item grid (includes filter input)
  renderItemGrid();

  ImGui::End();
}

void TilesetWidget::renderTilesetDropdown() {
  if (!tilesetRegistry_) {
    ImGui::TextDisabled(ICON_FA_BOX_OPEN " Registry not initialized");
    return;
  }
  const auto &allTilesets = tilesetRegistry_->getAllTilesets();

  if (allTilesets.empty()) {
    ImGui::TextDisabled(ICON_FA_BOX_OPEN " No tilesets loaded");
    return;
  }

  std::vector<int> duplicateCounts(allTilesets.size(), 0);
  for (size_t i = 0; i < allTilesets.size(); ++i) {
    duplicateCounts[i] = static_cast<int>(std::count_if(
        allTilesets.begin(), allTilesets.end(), [&](const auto &candidate) {
          return candidate && allTilesets[i] &&
                 candidate->getName() == allTilesets[i]->getName();
        }));
  }

  // Find current index
  int currentIdx = -1;
  for (size_t i = 0; i < allTilesets.size(); ++i) {
    if (allTilesets[i].get() == currentTileset_) {
      currentIdx = static_cast<int>(i);
      break;
    }
  }

  // Auto-select first if none selected
  if (currentIdx < 0 && !allTilesets.empty()) {
    currentIdx = 0;
    currentTileset_ = allTilesets[0].get();
    filterDirty_ = true;
  }

  const auto formatTilesetLabel = [&](size_t index) {
    const auto *tileset = allTilesets[index].get();
    if (!tileset) {
      return std::string{"<missing>"};
    }
    if (duplicateCounts[index] <= 1 || tileset->getSourceFile().empty()) {
      return tileset->getName();
    }
    return std::format("{} [{}]", tileset->getName(),
                       tileset->getSourceFile().stem().string());
  };

  const auto previewLabel =
      currentIdx >= 0 ? formatTilesetLabel(static_cast<size_t>(currentIdx))
                      : std::string{"Select tileset..."};

  // Full width dropdown
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##Tileset", previewLabel.c_str())) {
    for (size_t i = 0; i < allTilesets.size(); ++i) {
      bool isSelected = (static_cast<int>(i) == currentIdx);
      const auto label = formatTilesetLabel(i);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
        currentTileset_ = allTilesets[i].get();
        selectedTilesetIdx_ = static_cast<int>(i);
        filterDirty_ = true;
      }
      if (isSelected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  Utils::SetTooltipOnHover("Select Tileset");
}

void TilesetWidget::renderItemGrid() {
  if (!currentTileset_ || !tilesetRegistry_)
    return;

  const auto *tileset = currentTileset_;
  if (!tileset) {
    ImGui::TextDisabled(ICON_FA_TRIANGLE_EXCLAMATION " Tileset not found");
    return;
  }

  // Get all brushes from the flat tileset
  auto brushes = tileset->getBrushes();
  if (brushes.empty()) {
    ImGui::TextDisabled(ICON_FA_BOX_OPEN " No brushes in this tileset");
    return;
  }

  // Begin child first to handle scrollbar affecting width
  ImGui::BeginChild("ItemGrid", ImVec2(0, 0), true);

  // Update cached filter if dirty
  if (filterDirty_) {
    std::string lowerFilter = filterBuffer_;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lowerFilter.empty()) {
      filteredBrushes_.assign(brushes.begin(), brushes.end());
    } else {
      filteredBrushes_.clear();
      filteredBrushes_.reserve(brushes.size());

      std::copy_if(brushes.begin(), brushes.end(),
                   std::back_inserter(filteredBrushes_),
                   [&](const Brushes::IBrush *brush) {
                     const auto &brushName = brush->getName();
                     return std::search(brushName.begin(), brushName.end(),
                                        lowerFilter.begin(), lowerFilter.end(),
                                        [](unsigned char c1, unsigned char c2) {
                                          return std::tolower(c1) == c2;
                                        }) != brushName.end();
                   });
    }
    filterDirty_ = false;
  }

  int itemCount = static_cast<int>(filteredBrushes_.size());

  // Filter input row
  float availableWidth = ImGui::GetContentRegionAvail().x;
  ImGui::SetNextItemWidth(availableWidth - 30.0f);
  if (ImGui::InputTextWithHint("##Filter", ICON_FA_FILTER " Filter...",
                               filterBuffer_, sizeof(filterBuffer_))) {
    filterDirty_ = true;
  }
  Utils::SetTooltipOnHover("Filter brushes by name");

  if (!std::string_view(filterBuffer_).empty()) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_XMARK "##ClearFilter")) {
      filterBuffer_[0] = '\0';
      filterDirty_ = true;
    }
    Utils::SetTooltipOnHover("Clear filter");
  }

  if (itemCount == 0 && !std::string_view(filterBuffer_).empty()) {
    ImGui::TextDisabled(ICON_FA_FILTER_CIRCLE_XMARK " No brushes match filter");
  }

  ImGui::Separator();

  // Calculate grid layout
  availableWidth = ImGui::GetContentRegionAvail().x;
  float itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
  float actualItemWidth = iconSize_ + itemSpacingX;

  int columns =
      std::max(1, static_cast<int>(std::floor((availableWidth + itemSpacingX) /
                                              actualItemWidth)));
  int rows = (itemCount + columns - 1) / columns;

  ImGuiListClipper clipper;
  clipper.Begin(rows);

  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
      for (int col = 0; col < columns; col++) {
        int index = row * columns + col;
        if (index >= itemCount)
          break;

        const auto *brush = filteredBrushes_[index];
        if (!brush)
          continue;

        ImGui::PushID(index);

        std::string brushName = brush->getName();
        if (brushName.empty()) {
          brushName = "Unnamed";
        }

        bool isSelected = (selectedBrush_ == brush);

        ImVec2 tileSize(iconSize_, iconSize_);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("##tile", tileSize);
        bool isHovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();

        ImDrawList *dl = ImGui::GetWindowDrawList();

        ImU32 bgColor = IM_COL32(40, 40, 40, 255);
        if (isSelected) {
          bgColor = IM_COL32(60, 100, 160, 255);
        } else if (isHovered) {
          bgColor = IM_COL32(80, 80, 80, 255);
        }
        dl->AddRectFilled(
            cursorPos, ImVec2(cursorPos.x + tileSize.x, cursorPos.y + tileSize.y),
            bgColor);

        const auto preview = getBrushPreview(brush);
        Utils::RenderBrushPreviewTile(dl, cursorPos, tileSize, preview);

        if (isSelected) {
          dl->AddRect(
              cursorPos,
              ImVec2(cursorPos.x + tileSize.x, cursorPos.y + tileSize.y),
              IM_COL32(100, 180, 255, 255), 0, 0, 2.0f);
        }

        if (isHovered) {
          std::string tooltip = brush->getName();
          if (brush->getType() == Brushes::BrushType::Raw) {
            if (tooltip.empty())
              tooltip = "Raw Item";
            tooltip = std::format("{} (ID: {})", tooltip, brush->getLookId());
          } else if (tooltip.empty()) {
            tooltip = std::format("ID: {}", brush->getLookId());
          }
          ImGui::SetTooltip("%s", tooltip.c_str());
        }

        if (clicked) {
          selectedBrush_ = brush;

          if (brushController_) {
            brushController_->setBrush(const_cast<Brushes::IBrush *>(brush));
          }

          if (onBrushSelected_) {
            auto *rawBrush =
                dynamic_cast<const Brushes::RawBrush *>(brush);
            onBrushSelected_(rawBrush ? rawBrush->getItemId() : 0,
                             brush->getName());
          }
        }

        ImGui::PopID();

        if (col < columns - 1)
          ImGui::SameLine();
      }
    }
  }

  ImGui::EndChild();
}

void TilesetWidget::syncActiveBrushSelection() {
  if (!brushController_) {
    return;
  }
  const auto *activeBrush = brushController_->getCurrentBrush();
  if (activeBrush == syncedActiveBrush_) {
    return;
  }
  syncedActiveBrush_ = activeBrush;
  selectedBrush_ = activeBrush;

  if (const auto *tileset = findTilesetForBrush(activeBrush); tileset) {
    currentTileset_ = tileset;
    filterDirty_ = true;
  }

  updateSelectedTilesetIndex();
}

Utils::ResolvedBrushPreview TilesetWidget::getBrushPreview(
    const Brushes::IBrush *brush) const {
  return Utils::ResolveBrushPreview(brush, clientData_, spriteManager_);
}

const Domain::Tileset::Tileset *TilesetWidget::findTilesetForBrush(
    const Brushes::IBrush *brush) const {
  if (!tilesetRegistry_ || !brush) {
    return nullptr;
  }

  for (const auto &tileset : tilesetRegistry_->getAllTilesets()) {
    for (const auto &entry : tileset->getEntries()) {
      if (Domain::Tileset::isBrush(entry) &&
          Domain::Tileset::getBrush(entry) == brush) {
        return tileset.get();
      }
    }
  }

  return nullptr;
}

void TilesetWidget::updateSelectedTilesetIndex() {
  if (!tilesetRegistry_) {
    selectedTilesetIdx_ = 0;
    return;
  }

  const auto &tilesets = tilesetRegistry_->getAllTilesets();
  for (size_t i = 0; i < tilesets.size(); ++i) {
    if (tilesets[i].get() == currentTileset_) {
      selectedTilesetIdx_ = static_cast<int>(i);
      return;
    }
  }

  selectedTilesetIdx_ = 0;
}

void TilesetWidget::renderIconSizeSlider() {
  // Full width slider
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::SliderFloat("##IconSize", &iconSize_, 32.0f, 128.0f, "%.0f px");
  Utils::SetTooltipOnHover("Adjust icon size");
}

} // namespace MapEditor::UI
