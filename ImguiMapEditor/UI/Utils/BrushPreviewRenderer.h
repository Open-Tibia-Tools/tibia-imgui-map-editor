#pragma once

#include "BrushPreviewResolver.h"
#include <imgui.h>

namespace MapEditor::UI::Utils {

void RenderBrushPreviewTile(ImDrawList *drawList, const ImVec2 &cursorPos,
                            const ImVec2 &tileSize,
                            const ResolvedBrushPreview &preview);

} // namespace MapEditor::UI::Utils
