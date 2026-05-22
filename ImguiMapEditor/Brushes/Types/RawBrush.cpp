#include "RawBrush.h"
#include "Brushes/Behaviors/ItemPlacement.h"
#include "BrushUtils.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Position.h"
namespace MapEditor::Brushes {

RawBrush::RawBrush(uint32_t itemId, const Domain::ItemType* type)
    : BrushBase("RAW:" + std::to_string(itemId), itemId, true)
    , itemId_(itemId)
    , cachedType_(type) 
{
    // For raw brushes, lookId is the same as itemId
}

void RawBrush::draw(Domain::ChunkedMap& map, 
                    Domain::Tile* tile,
                    const DrawContext& ctx) 
{
    if (!tile) {
        return;
    }

    auto item = Types::createTypedItem(ctx, static_cast<uint16_t>(itemId_));
    if (!item) {
        item = std::make_unique<Domain::Item>(static_cast<uint16_t>(itemId_));
    } else if (!item->getType() && cachedType_) {
        item->setType(cachedType_);
        item->setClientId(cachedType_->client_id);
    }

    // Add to tile (sorting is handled by Tile::addItem)
    tile->addItem(std::move(item));
}

void RawBrush::undraw(Domain::ChunkedMap& map, 
                      Domain::Tile* tile) 
{
    if (!tile) {
        return;
    }
    
    // Remove all items matching this brush's itemId
    // This matches RME's RAWBrush::undraw behavior
    tile->removeItemsIf([this](const Domain::Item* item) {
        return ownsItem(item);
    });
}

bool RawBrush::ownsItem(const Domain::Item* item) const {
    if (!item) {
        return false;
    }
    return item->getServerId() == itemId_;
}

} // namespace MapEditor::Brushes
