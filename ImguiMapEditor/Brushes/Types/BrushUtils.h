#pragma once

#include "Brushes/Core/IBrush.h"
#include <memory>

namespace MapEditor::Domain {
class Item;
class Tile;
} // namespace MapEditor::Domain

namespace MapEditor::Brushes::Types {

std::unique_ptr<Domain::Item> createTypedItem(const DrawContext &ctx,
                                              uint16_t itemId,
                                              uint16_t subtype = 1);

void updateItemVisuals(Domain::Item &item,
                       Services::ClientDataService *clientData,
                       uint16_t itemId, BrushId ownerBrushId);

void updateItemVisuals(Domain::Item &item, BrushRegistry &registry,
                       uint16_t itemId, BrushId ownerBrushId);

bool itemBlocksPlacement(const Domain::Item *item);
bool tileHasBlockingContents(const Domain::Tile *tile);

} // namespace MapEditor::Brushes::Types
