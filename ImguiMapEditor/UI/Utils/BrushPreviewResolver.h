#pragma once

#include "Brushes/Core/IBrush.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"

namespace MapEditor::Domain {
class ItemType;
struct Outfit;
} // namespace MapEditor::Domain

namespace MapEditor::Rendering {
class Texture;
} // namespace MapEditor::Rendering

namespace MapEditor::UI::Utils {

struct ResolvedBrushPreview {
  Rendering::Texture *texture = nullptr;
  bool isCreature = false;
  bool isItem = false;
  uint32_t previewId = 0;
  std::string fallbackLabel;
};

ResolvedBrushPreview ResolveBrushPreview(
    const Brushes::IBrush *brush, Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager);

} // namespace MapEditor::UI::Utils
