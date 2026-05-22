#include "PaletteWindow.h"

#include "../../Brushes/BrushController.h"

#include <algorithm>
#include <format>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "../../Brushes/BrushController.h"
#include "../../Domain/Palette/Palette.h"
#include "../../Domain/Tileset/TilesetEntry.h"
#include "../../Domain/Tileset/TilesetRegistry.h"
#include "../../IO/TilesetXmlWriter.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"

namespace MapEditor::UI {

using namespace Domain::Palette;
using namespace Domain::Tileset;

PaletteWindow::PaletteWindow(const std::string &paletteName)
    : paletteName_(paletteName) {}

PaletteWindow::~PaletteWindow() = default;

void PaletteWindow::initialize(
    Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager,
    Brushes::BrushController *brushController,
    Domain::Tileset::TilesetRegistry &tilesetRegistry,
    Domain::Palette::PaletteRegistry &paletteRegistry,
    Services::AppSettings *appSettings) {
  clientData_ = clientData;
  spriteManager_ = spriteManager;
  brushController_ = brushController;
  appSettings_ = appSettings;
  tilesetRegistry_ = &tilesetRegistry;
  paletteRegistry_ = &paletteRegistry;

  gridWidget_.initialize(clientData, spriteManager, brushController,
                         tilesetRegistry, appSettings);

  // Wire up double-click callback for jump-to-tileset
  gridWidget_.setOnBrushDoubleClicked(
      [this](const std::string &tilesetName, const std::string &brushName) {
        handleJumpToTileset(tilesetName, brushName);
      });

  // Wire up tileset modification callback for auto-save
  // Capture tilesetRegistry_ to use in closure
  auto *tilesetReg = tilesetRegistry_;
  gridWidget_.setOnTilesetModified(
      [tilesetReg](const Domain::Tileset::Tileset &tileset) {
        if (!tilesetReg)
          return;
        auto *resolvedTileset =
            tilesetReg->getTilesetBySourceFile(tileset.getSourceFile());
        if (resolvedTileset) {
          if (!resolvedTileset->getSourceFile().empty()) {
            spdlog::info("[PaletteWindow] Saving tileset '{}' to: {}",
                         resolvedTileset->getName(),
                         resolvedTileset->getSourceFile().string());
            bool success = IO::TilesetXmlWriter::write(
                resolvedTileset->getSourceFile(), *resolvedTileset);
            if (success) {
              resolvedTileset->clearDirty();
              spdlog::info("[PaletteWindow] Saved successfully");
            } else {
              spdlog::error("[PaletteWindow] Failed to save tileset");
            }
          } else {
            spdlog::warn("[PaletteWindow] Tileset '{}' has no source file set",
                         resolvedTileset->getName());
          }
        }
      });

  // Get palette from registry (using injected member)
  palette_ = paletteRegistry_->getPalette(paletteName_);

  // Collect all brushes from all tilesets for cross-tileset search
  std::vector<BrushWithSource> allBrushes;
  if (palette_) {
    for (const auto *tileset : palette_->getTilesets()) {
      for (const auto &entry : tileset->getEntries()) {
        if (Domain::Tileset::isBrush(entry)) {
          const auto *brush = Domain::Tileset::getBrush(entry);
          if (brush) {
            allBrushes.push_back({brush, tileset->getName()});
          }
        }
      }
    }
  }
  gridWidget_.setAllBrushes(std::move(allBrushes));

  refreshTilesetList();
  initialized_ = true;
}

void PaletteWindow::refreshTilesetList() {
  tilesetNames_.clear();

  if (!palette_) {
    spdlog::warn("[PaletteWindow] Palette '{}' not found in registry",
                 paletteName_);
    return;
  }

  tilesetNames_ = palette_->getTilesetNames();

  // Auto-select first tileset
  if (!tilesetNames_.empty() && selectedTilesetIndex_ == 0) {
    selectTileset(0);
  }
}

void PaletteWindow::selectTileset(size_t index) {
  if (index >= tilesetNames_.size()) {
    return;
  }

  selectedTilesetIndex_ = static_cast<int>(index);

  // Get the tileset and set it on the grid
  Domain::Tileset::Tileset *tileset = getTilesetAt(index);
  if (tileset) {
    gridWidget_.setTileset(tileset);
  }
}

void PaletteWindow::handleJumpToTileset(const std::string &tilesetName,
                                        const std::string &brushName) {
  gridWidget_.clearFilter();

  // Find tileset index and switch to it
  for (size_t i = 0; i < tilesetNames_.size(); ++i) {
    if (tilesetNames_[i] == tilesetName) {
      selectTileset(i);
      break;
    }
  }

  gridWidget_.selectBrush(brushName, true, true); // scrollTo=true, pulse=true
}

bool PaletteWindow::render() {
  if (!visible_) {
    return true;
  }

  if (!initialized_) {
    return true;
  }

  // Window title with palette name
  std::string windowTitle =
      std::format("{} {}##{}", ICON_FA_PALETTE, paletteName_, paletteName_);

  ImGui::SetNextWindowSize(ImVec2(350, 450), ImGuiCond_FirstUseEver);

  bool windowOpen = true;
  if (ImGui::Begin(windowTitle.c_str(), &windowOpen)) {
    if (tilesetNames_.empty()) {
      ImGui::TextDisabled(ICON_FA_BOX_OPEN " No tilesets in this palette");
    } else {
      syncActiveBrushSelection();
      // Get available space to determine layout
      ImVec2 availableSize = ImGui::GetContentRegionAvail();
      bool useHorizontalLayout = availableSize.x > availableSize.y * 1.3f;

      const float controlsPanelWidth = 150.0f;

      if (useHorizontalLayout) {
        // === HORIZONTAL LAYOUT ===
        if (ImGui::BeginChild("ControlsPanel", ImVec2(controlsPanelWidth, -1),
                              true)) {
          gridWidget_.renderControlsOnly(true);
          ImGui::Separator();

          float tilesetListHeight = ImGui::GetContentRegionAvail().y;
          if (ImGui::BeginChild("TilesetList", ImVec2(-1, tilesetListHeight),
                                false)) {
            for (size_t i = 0; i < tilesetNames_.size(); ++i) {
              bool isSelected = (selectedTilesetIndex_ == static_cast<int>(i));
              const auto label = getTilesetDisplayLabel(i);

              if (isSelected) {
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
              }

              if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                selectTileset(i);
              }

              if (isSelected) {
                ImGui::PopStyleColor();
              }
            }
          }
          ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("GridPanel", ImVec2(0, -1), true)) {
          gridWidget_.renderGridOnly();
        }
        ImGui::EndChild();

      } else {
        // === VERTICAL LAYOUT ===
        ImGui::SetNextItemWidth(-1);
        if (selectedTilesetIndex_ >= static_cast<int>(tilesetNames_.size())) {
          selectedTilesetIndex_ =
              tilesetNames_.empty()
                  ? 0
                  : static_cast<int>(tilesetNames_.size()) - 1;
        }

        const auto previewLabel = tilesetNames_.empty()
                                      ? std::string{}
                                      : getTilesetDisplayLabel(
                                            static_cast<size_t>(
                                                selectedTilesetIndex_));
        if (!tilesetNames_.empty() &&
            ImGui::BeginCombo("##TilesetCombo", previewLabel.c_str())) {
          for (size_t i = 0; i < tilesetNames_.size(); ++i) {
            bool isSelected = (selectedTilesetIndex_ == static_cast<int>(i));
            const auto label = getTilesetDisplayLabel(i);
            if (ImGui::Selectable(label.c_str(), isSelected)) {
              selectTileset(i);
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        ImGui::Spacing();
        gridWidget_.renderControlsOnly(false);
        ImGui::Separator();
        ImGui::Spacing();
        gridWidget_.renderGridOnly();
      }
    }
  }
  ImGui::End();

  if (!windowOpen) {
    visible_ = false;
  }

  return true;
}

Domain::Tileset::Tileset *PaletteWindow::getTilesetAt(size_t index) const {
  if (!palette_) {
    return nullptr;
  }
  return palette_->getTilesetAt(index);
}

std::string PaletteWindow::getTilesetDisplayLabel(size_t index) const {
  if (index >= tilesetNames_.size()) {
    return {};
  }

  const auto &name = tilesetNames_[index];
  const auto duplicateCount = static_cast<int>(std::count(
      tilesetNames_.begin(), tilesetNames_.end(), name));
  if (duplicateCount <= 1) {
    return std::format("{}##tileset_{}", name, index);
  }

  const auto *tileset = getTilesetAt(index);
  const auto sourceStem =
      tileset && !tileset->getSourceFile().empty()
          ? tileset->getSourceFile().stem().string()
          : std::format("{}", index);
  return std::format("{} [{}]##tileset_{}", name, sourceStem, index);
}

std::optional<size_t> PaletteWindow::findTilesetIndexForBrush(
    const Brushes::IBrush *brush) const {
  if (!brush || !palette_) {
    return std::nullopt;
  }

  const auto &tilesets = palette_->getTilesets();
  for (size_t i = 0; i < tilesets.size(); ++i) {
    const auto *tileset = tilesets[i];
    if (!tileset) {
      continue;
    }

    for (const auto &entry : tileset->getEntries()) {
      if (Domain::Tileset::isBrush(entry) &&
          Domain::Tileset::getBrush(entry) == brush) {
        return i;
      }
    }
  }

  return std::nullopt;
}

void PaletteWindow::syncActiveBrushSelection() {
  if (!brushController_) {
    return;
  }
  const auto *activeBrush = brushController_->getCurrentBrush();
  if (activeBrush == syncedActiveBrush_) {
    return;
  }
  syncedActiveBrush_ = activeBrush;
  if (!activeBrush) {
    return;
  }

  if (auto index = findTilesetIndexForBrush(activeBrush)) {
    selectTileset(*index);
  }
  gridWidget_.selectBrush(activeBrush, false, false);
}

} // namespace MapEditor::UI
