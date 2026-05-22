#include "BrushPreviewResolver.h"

#include "Brushes/Types/CarpetBrush.h"
#include "Brushes/Types/DoorBrush.h"
#include "Brushes/Types/GroundBrush.h"
#include "Brushes/Types/RawBrush.h"
#include "Brushes/Types/TableBrush.h"
#include "Brushes/Types/WallBrush.h"
#include "PreviewUtils.hpp"
#include <optional>

namespace MapEditor::UI::Utils {

namespace {

std::string fallbackLabelForBrush(const Brushes::IBrush &brush) {
  if (!brush.getName().empty()) {
    return brush.getName();
  }

  switch (brush.getType()) {
  case Brushes::BrushType::Spawn:
    return "Spawn";
  case Brushes::BrushType::House:
    return "House";
  case Brushes::BrushType::HouseExit:
    return "Exit";
  case Brushes::BrushType::Waypoint:
    return "WP";
  case Brushes::BrushType::Flag:
    return "Zone";
  case Brushes::BrushType::OptionalBorder:
    return "Border";
  case Brushes::BrushType::Eraser:
    return "Erase";
  case Brushes::BrushType::Door:
    return "Door";
  case Brushes::BrushType::Placeholder:
    return "Missing";
  default:
    return "?";
  }
}

ResolvedBrushPreview resolveServerItemPreview(
    const Brushes::IBrush &brush, uint16_t itemId,
    Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager) {
  if (!clientData || !spriteManager || itemId == 0) {
    return {.fallbackLabel = fallbackLabelForBrush(brush)};
  }

  const auto *itemType = clientData->getItemTypeByServerId(itemId);
  if (auto *texture = GetItemPreview(*spriteManager, itemType)) {
    return {.texture = texture,
            .isItem = true,
            .previewId = itemId,
            .fallbackLabel = fallbackLabelForBrush(brush)};
  }

  return {.fallbackLabel = fallbackLabelForBrush(brush)};
}

std::optional<uint16_t> fallbackServerItemId(const Brushes::IBrush &brush) {
  if (auto *rawBrush = dynamic_cast<const Brushes::RawBrush *>(&brush)) {
    return rawBrush->getItemId();
  }
  if (auto *groundBrush = dynamic_cast<const Brushes::GroundBrush *>(&brush)) {
    return groundBrush->getPreviewItemId();
  }
  if (auto *wallBrush = dynamic_cast<const Brushes::WallBrush *>(&brush)) {
    return wallBrush->getPreviewItemId();
  }
  if (auto *carpetBrush = dynamic_cast<const Brushes::CarpetBrush *>(&brush)) {
    return carpetBrush->getPreviewItemId();
  }
  if (auto *tableBrush = dynamic_cast<const Brushes::TableBrush *>(&brush)) {
    return tableBrush->getPreviewItemId();
  }
  if (auto *doorBrush = dynamic_cast<const Brushes::DoorBrush *>(&brush)) {
    const auto lookId = static_cast<uint16_t>(doorBrush->getLookId());
    return lookId != 0 ? std::optional<uint16_t>{lookId} : std::nullopt;
  }
  if (const auto lookId = static_cast<uint16_t>(brush.getLookId()); lookId != 0) {
    return lookId;
  }
  return std::nullopt;
}

} // namespace

ResolvedBrushPreview ResolveBrushPreview(const Brushes::IBrush *brush,
                                         Services::ClientDataService *clientData,
                                         Services::SpriteManager *spriteManager) {
  if (!brush) {
    return {};
  }

  const auto preview = brush->getPreviewDescriptor();
  switch (preview.kind) {
  case Brushes::BrushPreviewKind::ServerItem: {
    return resolveServerItemPreview(*brush,
                                    static_cast<uint16_t>(preview.numericId),
                                    clientData, spriteManager);
    break;
  }
  case Brushes::BrushPreviewKind::ClientSprite: {
    if (!spriteManager) {
      break;
    }

    auto &texture =
        spriteManager->getOverlaySpriteCache().getTextureOrPlaceholder(
            preview.numericId);
    return {.texture = &texture,
            .previewId = preview.numericId,
            .fallbackLabel = fallbackLabelForBrush(*brush)};
  }
  case Brushes::BrushPreviewKind::Creature: {
    if (!clientData || !spriteManager) {
      break;
    }

    auto creaturePreview =
        GetCreaturePreview(*clientData, *spriteManager, preview.outfit);
    if (creaturePreview.texture) {
      return {.texture = creaturePreview.texture,
              .isCreature = true,
              .previewId = static_cast<uint32_t>(preview.outfit.lookType),
              .fallbackLabel = fallbackLabelForBrush(*brush)};
    }
    break;
  }
  case Brushes::BrushPreviewKind::Symbolic:
    return {.fallbackLabel = preview.label.empty() ? fallbackLabelForBrush(*brush)
                                                   : preview.label};
  case Brushes::BrushPreviewKind::None:
    break;
  }

  if (const auto itemId = fallbackServerItemId(*brush); itemId.has_value()) {
    const auto resolved =
        resolveServerItemPreview(*brush, *itemId, clientData, spriteManager);
    if (resolved.texture) {
      return resolved;
    }
  }

  return {.fallbackLabel = fallbackLabelForBrush(*brush)};
}

} // namespace MapEditor::UI::Utils
