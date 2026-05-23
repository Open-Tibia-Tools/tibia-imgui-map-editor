#pragma once
#include "../BinaryReader.h"
#include "Domain/ItemType.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace MapEditor {
namespace IO {

/**
 * Item category in DAT file
 */
enum class DatCategory : uint8_t {
    Item = 0,
    Outfit = 1,
    Effect = 2,
    Missile = 3
};

/**
 * Client item data loaded from DAT
 */
struct ClientItem {
    uint16_t id = 0;
    DatCategory category = DatCategory::Item;
    
    // Sprite dimensions
    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;
    
    // Sprite IDs
    std::vector<uint32_t> sprite_ids;
    
    // Properties from flags
    bool is_ground = false;
    uint16_t ground_speed = 0;
    bool is_on_bottom = false;
    bool is_on_top = false;
    bool is_container = false;
    bool is_stackable = false;
    bool is_useable = false;
    bool is_writable = false;
    uint16_t max_text_length = 0;
    bool is_fluid_container = false;
    bool is_fluid = false;
    bool is_unpassable = false;
    bool is_unmoveable = false;
    bool blocks_missiles = false;
    bool blocks_pathfinder = false;
    bool is_pickupable = false;
    bool is_hangable = false;
    bool is_horizontal = false;
    bool is_vertical = false;
    bool is_rotatable = false;
    bool has_light = false;
    uint16_t light_level = 0;
    uint16_t light_color = 0;
    bool dont_hide = false;
    bool is_translucent = false;
    bool has_offset = false;
    int16_t offset_x = 0;
    int16_t offset_y = 0;
    bool has_elevation = false;
    uint16_t elevation = 0;
    bool is_lying_object = false;
    bool animate_always = false;
    bool has_minimap_color = false;
    uint16_t minimap_color = 0;
    bool full_ground = false;
    bool ignore_look = false;
    bool is_cloth = false;
    uint16_t cloth_slot = 0;
    bool has_market_data = false;
    uint16_t market_category = 0;
    uint16_t trade_as = 0;
    uint16_t show_as = 0;
    std::string market_name;
    uint16_t market_profession = 0;
    uint16_t market_level = 0;
    bool has_default_action = false;
    uint16_t default_action = 0;
    bool floor_change = false;
    uint16_t lens_help = 0;
    bool wrappable = false;
    bool unwrappable = false;
    bool top_effect = false;
    bool no_move_animation = false;
    bool usable = false;
    
    // Animation data (10.50+)
    bool has_animation_data = false;
    uint8_t animation_mode = 0;
    int32_t loop_count = 0;
    uint8_t start_frame = 0;
    std::vector<std::pair<uint32_t, uint32_t>> frame_durations;
    
    // Calculate total sprite count
    uint32_t getTotalSprites() const {
        return static_cast<uint32_t>(width) * height * layers *
               pattern_x * pattern_y * pattern_z * frames;
    }
};

/**
 * Result of DAT parsing
 */
struct DatResult {
    bool success = false;
    uint32_t signature = 0;
    uint16_t max_item_id = 0;
    uint16_t max_outfit_id = 0;
    uint16_t max_effect_id = 0;
    uint16_t max_missile_id = 0;
    
    std::vector<ClientItem> items;
    std::vector<ClientItem> outfits;
    std::vector<ClientItem> effects;
    std::vector<ClientItem> missiles;
    
    std::string error;
};

/**
 * Abstract base class for version-specific DAT readers
 */
class DatReaderBase {
public:
    virtual ~DatReaderBase() = default;
    
    /**
     * Read DAT file
     */
    DatResult read(const std::filesystem::path& path,
                   uint32_t expected_signature = 0);
    
    /**
     * Get version range this reader supports
     */
    virtual std::pair<uint32_t, uint32_t> getVersionRange() const = 0;
    
    /**
     * Get reader name for debugging
     */
    virtual const char* getName() const = 0;

    /**
     * Whether this version uses extended (32-bit) sprite IDs
     */
    virtual bool usesExtendedSprites() const { return false; }
    
    /**
     * Whether this version has frame duration data
     */
    virtual bool hasFrameDurations() const { return false; }
    
    /**
     * Whether this version uses frame groups (10.50+ for outfits)
     */
    virtual bool hasFrameGroups() const { return false; }

protected:
    /**
     * Version-specific flag reading.
     * Default implementation uses a loop that calls transformFlag, handles standard flags,
     * and calls handleSpecificFlag for custom ones.
     */
    virtual void readItemFlags(ClientItem& item, BinaryReader& reader);

    /**
     * Transforms a raw flag to a canonical flag.
     * Override this to map version-specific flags to CanonicalFlags.
     */
    virtual uint8_t transformFlag(uint8_t raw) { return raw; }

    /**
     * Handles flags that are not covered by the standard switch case or require
     * version-specific processing (like HAS_OFFSET in older versions).
     * Returns true if the flag was handled, false otherwise.
     */
    virtual bool handleSpecificFlag(uint8_t flag, ClientItem& item, BinaryReader& reader) { return false; }
    
    /**
     * Whether this version reads patternZ from file (false for 7.10-7.54)
     */
    virtual bool shouldReadPatternZ() const { return true; }

private:
    void readCategory(BinaryReader& reader, std::vector<ClientItem>& items,
                      DatCategory category, uint16_t min_id, uint16_t max_id);
    void readSpriteData(ClientItem& item, BinaryReader& reader);
};

} // namespace IO
} // namespace MapEditor
