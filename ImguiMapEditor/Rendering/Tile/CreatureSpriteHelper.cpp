#include "Rendering/Tile/CreatureSpriteHelper.h"
#include "Rendering/Core/Texture.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Domain/CreatureType.h"
#include "IO/Readers/DatReaderBase.h"
namespace MapEditor::Rendering {

CreatureSpriteHelper::CreatureSpriteHelper(Services::ClientDataService* client_data,
                                           Services::SpriteManager* sprite_manager)
    : client_data_(client_data)
    , sprite_manager_(sprite_manager)
{
}

Texture* CreatureSpriteHelper::getThumbnail(const std::string& creature_name) {
    if (!isValid() || creature_name.empty()) {
        return nullptr;
    }
    
    // Look up creature type by name
    const Domain::CreatureType* creature_type = client_data_->getCreatureType(creature_name);
    if (!creature_type || creature_type->outfit.lookType == 0) {
        return nullptr;
    }
    
    return getThumbnail(creature_type->outfit);
}

Texture* CreatureSpriteHelper::getThumbnail(const Domain::Outfit& outfit) {
    if (!isValid() || outfit.lookType == 0) {
        return nullptr;
    }
    
    // Get outfit data (ClientItem) for this lookType
    const IO::ClientItem* outfit_data = getOutfitData(outfit.lookType);
    if (!outfit_data) {
        return nullptr;
    }
    
    // Use SpriteManager's composited creature texture API
    return sprite_manager_->getCreatureSpriteService().getCompositedCreatureTexture(
        outfit_data,
        static_cast<uint8_t>(outfit.lookHead),
        static_cast<uint8_t>(outfit.lookBody),
        static_cast<uint8_t>(outfit.lookLegs),
        static_cast<uint8_t>(outfit.lookFeet)
    );
}

float CreatureSpriteHelper::getRecommendedSize(const std::string& creature_name) const {
    if (!client_data_ || creature_name.empty()) {
        return 32.0f;
    }
    
    const Domain::CreatureType* creature_type = client_data_->getCreatureType(creature_name);
    if (!creature_type || creature_type->outfit.lookType == 0) {
        return 32.0f;
    }
    
    return getRecommendedSize(creature_type->outfit);
}

float CreatureSpriteHelper::getRecommendedSize(const Domain::Outfit& outfit) const {
    if (!client_data_ || outfit.lookType == 0) {
        return 32.0f;
    }
    
    const IO::ClientItem* outfit_data = getOutfitData(outfit.lookType);
    if (!outfit_data) {
        return 32.0f;
    }
    
    // Return max dimension * 32 for proper aspect ratio
    return static_cast<float>(std::max(outfit_data->width, outfit_data->height) * 32);
}

CreatureSpriteHelper::SpriteResult CreatureSpriteHelper::resolveSpriteId(
    const Domain::Outfit& outfit,
    uint8_t direction,
    int animation_frame)
{
    SpriteResult result;
    
    if (!client_data_ || outfit.lookType == 0) {
        return result;
    }
    
    const IO::ClientItem* outfit_data = getOutfitData(outfit.lookType);
    if (!outfit_data || outfit_data->sprite_ids.empty()) {
        return result;
    }
    
    // Store colors for GPU colorization
    result.head = static_cast<uint8_t>(outfit.lookHead);
    result.body = static_cast<uint8_t>(outfit.lookBody);
    result.legs = static_cast<uint8_t>(outfit.lookLegs);
    result.feet = static_cast<uint8_t>(outfit.lookFeet);
    result.width = outfit_data->width;
    result.height = outfit_data->height;
    
    // Calculate sprite index based on direction and animation
    // Pattern: direction (0-3), pattern_y (addons), pattern_z (mount)
    size_t const pattern_x = direction % outfit_data->pattern_x;
    size_t const frame = animation_frame % outfit_data->frames;
    
    // Calculate linear index into sprite_ids array
    // Index = frame * (pattern_x * pattern_y * pattern_z * width * height)
    //       + pattern_z * (pattern_x * pattern_y * width * height)
    //       + pattern_y * (pattern_x * width * height)
    //       + pattern_x * (width * height)
    //       + cy * width + cx
    size_t const sprites_per_frame = outfit_data->pattern_x * outfit_data->pattern_y * 
                               outfit_data->pattern_z * outfit_data->width * outfit_data->height;
    size_t const base_index = frame * sprites_per_frame + pattern_x * outfit_data->width * outfit_data->height;
    
    if (base_index < outfit_data->sprite_ids.size()) {
        result.sprite_id = outfit_data->sprite_ids[base_index];
    }
    
    // Check if outfit needs colorization (has template layer)
    // Outfits with layers > 1 have template masks for colorization
    if (outfit_data->layers > 1) {
        result.needs_colorization = true;
        // Template is at layer 1 (second layer)
        size_t const template_offset = sprites_per_frame;
        if (base_index + template_offset < outfit_data->sprite_ids.size()) {
            result.template_sprite_id = outfit_data->sprite_ids[base_index + template_offset];
        }
    }
    
    return result;
}

const IO::ClientItem* CreatureSpriteHelper::getOutfitData(uint16_t look_type) const {
    if (!client_data_ || look_type == 0) {
        return nullptr;
    }
    return client_data_->getOutfitData(look_type);
}

} // namespace MapEditor::Rendering
