#include "ItemPlacement.h"
#include "Brushes/Core/IBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include "Domain/Item.h"
#include "Domain/Position.h"
namespace MapEditor::Brushes {

Domain::Item* ItemPlacement::placeItem(
    Domain::ChunkedMap& map,
    const Domain::Position& pos,
    uint32_t itemId,
    uint16_t subtype) 
{
    // Get or create tile at position
    Domain::Tile* tile = map.getOrCreateTile(pos);
    if (!tile) {
        return nullptr;
    }
    
    // Create the item
    auto item = std::make_unique<Domain::Item>(
        static_cast<uint16_t>(itemId), 
        subtype > 0 ? subtype : static_cast<uint16_t>(1)
    );
    
    Domain::Item * result = item.get();
    
    // Add to tile (sorting is handled by Tile::addItem)
    tile->addItem(std::move(item));
    
    return result;
}

size_t ItemPlacement::removeItemsById(Domain::Tile* tile, uint32_t itemId) {
    if (!tile) {
        return 0;
    }
    
    return tile->removeItemsIf([itemId](const Domain::Item* item) {
        return item && item->getServerId() == itemId;
    });
}

size_t ItemPlacement::removeItemsByBrush(Domain::Tile* tile, const IBrush* brush) {
    if (!tile || !brush) {
        return 0;
    }
    
    return tile->removeItemsIf([brush](const Domain::Item* item) {
        return brush->ownsItem(item);
    });
}

} // namespace MapEditor::Brushes
