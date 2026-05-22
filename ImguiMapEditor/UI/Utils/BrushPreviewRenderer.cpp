#include "BrushPreviewRenderer.h"

#include <cctype>
#include <cstdint>
#include <imgui.h>
#include <string>

namespace MapEditor::UI::Utils {

namespace {

std::string compactFallbackLabel(std::string label) {
  if (label.empty()) {
    return "?";
  }

  if (label.size() <= 3) {
    return label;
  }

  std::string compact;
  compact.reserve(3);
  bool takeNext = true;
  for (const char ch : label) {
    if (std::isspace(static_cast<unsigned char>(ch)) ||
        ch == '_' || ch == '-') {
      takeNext = true;
      continue;
    }

    if (takeNext) {
      compact.push_back(static_cast<char>(std::toupper(
          static_cast<unsigned char>(ch))));
      takeNext = false;
      if (compact.size() == 3) {
        return compact;
      }
    }
  }

  if (compact.empty()) {
    compact.assign(label.begin(), label.begin() + std::min<size_t>(3, label.size()));
  }

  return compact;
}

} // namespace

void RenderBrushPreviewTile(ImDrawList *drawList, const ImVec2 &cursorPos,
                            const ImVec2 &tileSize,
                            const ResolvedBrushPreview &preview) {
  if (!drawList) {
    return;
  }

  if (preview.texture && preview.texture->isValid()) {
    drawList->AddImage(
        reinterpret_cast<void *>(static_cast<uintptr_t>(preview.texture->id())),
        cursorPos, ImVec2(cursorPos.x + tileSize.x, cursorPos.y + tileSize.y));
    return;
  }

  const std::string label = compactFallbackLabel(
      preview.fallbackLabel.empty() ? "?" : preview.fallbackLabel);
  drawList->AddRectFilled(
      cursorPos, ImVec2(cursorPos.x + tileSize.x, cursorPos.y + tileSize.y),
      IM_COL32(58, 58, 58, 255));
  drawList->AddRect(
      cursorPos, ImVec2(cursorPos.x + tileSize.x, cursorPos.y + tileSize.y),
      IM_COL32(120, 120, 120, 255));
  ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
  ImVec2 textPos{cursorPos.x + (tileSize.x - textSize.x) * 0.5f,
                 cursorPos.y + (tileSize.y - textSize.y) * 0.5f};
  drawList->AddText(textPos, IM_COL32(220, 220, 220, 255), label.c_str());
}

} // namespace MapEditor::UI::Utils
