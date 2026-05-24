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
#include "../../Brushes/Types/CreatureBrush.h"
#include "../../Brushes/Types/RawBrush.h"
#include "../../Domain/ItemType.h"
#include "../../Rendering/Core/Texture.h"
#include "../../Services/ClientDataService.h"
#include "../../Services/SpriteManager.h"
#include "../Utils/PreviewUtils.hpp"
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

  // Build list of tileset names
  std::vector<std::string> tilesetNames;
  tilesetNames.reserve(allTilesets.size());
  for (const auto &ts : allTilesets) {
    tilesetNames.push_back(ts->getName());
  }

  // Find current index
  int currentIdx = -1;
  for (size_t i = 0; i < tilesetNames.size(); ++i) {
    if (tilesetNames[i] == currentTilesetName_) {
      currentIdx = static_cast<int>(i);
      break;
    }
  }

  // Auto-select first if none selected
  if (currentIdx < 0 && !tilesetNames.empty()) {
    currentIdx = 0;
    currentTilesetName_ = tilesetNames[0];
    filterDirty_ = true;
  }

  const char *previewValue =
      currentIdx >= 0 ? tilesetNames[currentIdx].c_str() : "Select tileset...";

  // Full width dropdown
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##Tileset", previewValue)) {
    for (size_t i = 0; i < tilesetNames.size(); ++i) {
      bool const isSelected = (static_cast<int>(i) == currentIdx);
      if (ImGui::Selectable(tilesetNames[i].c_str(), isSelected)) {
        currentTilesetName_ = tilesetNames[i];
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
  if (currentTilesetName_.empty() || !tilesetRegistry_)
    return;

  const auto *tileset = tilesetRegistry_->getTileset(currentTilesetName_);
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

  int const itemCount = static_cast<int>(filteredBrushes_.size());

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
  float const itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
  float const actualItemWidth = iconSize_ + itemSpacingX;

  int const columns =
      std::max(1, static_cast<int>(std::floor((availableWidth + itemSpacingX) /
                                              actualItemWidth)));
  int const rows = (itemCount + columns - 1) / columns;

  ImGuiListClipper clipper;
  clipper.Begin(rows);

  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
      for (int col = 0; col < columns; col++) {
        int const index = row * columns + col;
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

        bool const isSelected = (selectedBrushName_ == brushName);
        bool clicked = false;

        // Render preview based on brush type
        uint32_t lookId = brush->getLookId();
        Brushes::BrushType const brushType = brush->getType();

        if (brushType == Brushes::BrushType::Creature) {
          // Render creature brush with sprite
          const auto *creatureBrush =
              static_cast<const Brushes::CreatureBrush *>(brush);
          const auto &outfit = creatureBrush->getOutfit();

          bool rendered = false;
          if (clientData_ && spriteManager_) {
            auto preview = Utils::GetCreaturePreview(*clientData_,
                                                     *spriteManager_, outfit);
            if (preview && preview.texture && preview.texture->isValid() &&
                preview.texture->id() != 0) {
              clicked =
                  Utils::RenderGridItem((void *)(intptr_t)preview.texture->id(),
                                        iconSize_, isSelected);
              rendered = true;
            }
          }

          if (!rendered) {
            clicked =
                ImGui::Button(brushName.c_str(), ImVec2(iconSize_, iconSize_));
          }
        } else {
          // Item-based brushes (Raw, Doodad, etc.)
          const Domain::ItemType *itemType = nullptr;
          if (clientData_) {
            itemType = clientData_->getItemTypeByServerId(
                static_cast<uint16_t>(lookId));
          }

          bool rendered = false;
          if (spriteManager_) {
            if (auto *texture =
                    Utils::GetItemPreview(*spriteManager_, itemType)) {
              clicked = Utils::RenderGridItem((void *)(intptr_t)texture->id(),
                                              iconSize_, isSelected);
              rendered = true;
            }
          }

          if (!rendered) {
            if (spriteManager_ && itemType) {
              clicked = ImGui::Button(brush->getName().c_str(),
                                      ImVec2(iconSize_, iconSize_));
            } else if (brush->getType() == Brushes::BrushType::Placeholder) {
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
              clicked = ImGui::Button(brush->getName().c_str(),
                                      ImVec2(iconSize_, iconSize_));
              ImGui::PopStyleColor();
              Utils::SetTooltipOnHover(
                  std::format("Missing data for brush: {}", brush->getName())
                      .c_str());
            } else {
              clicked = ImGui::Button(
                  brush->getName().empty() ? "?" : brush->getName().c_str(),
                  ImVec2(iconSize_, iconSize_));
            }
          }
        }

        if (ImGui::IsItemHovered()) {
          std::string tooltip = brush->getName();
          if (brush->getType() == Brushes::BrushType::Raw) {
            if (tooltip.empty())
              tooltip = "Raw Item";
            tooltip = std::format("{} (ID: {})", tooltip, lookId);
          } else if (tooltip.empty()) {
            tooltip = std::format("ID: {}", lookId);
          }
          ImGui::SetTooltip("%s", tooltip.c_str());
        }

        if (clicked) {
          selectedBrushName_ = brush->getName();

          // Activate brush - factory handles preview creation
          if (brushController_) {
            brushController_->setBrush(const_cast<Brushes::IBrush *>(brush));
          }

          if (onBrushSelected_) {
            auto *rawBrush = dynamic_cast<const Brushes::RawBrush *>(brush);
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

void TilesetWidget::renderIconSizeSlider() {
  // Full width slider
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::SliderFloat("##IconSize", &iconSize_, 32.0f, 128.0f, "%.0f px");
  Utils::SetTooltipOnHover("Adjust icon size");
}

} // namespace MapEditor::UI
