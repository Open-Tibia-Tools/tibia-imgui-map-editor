#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <unordered_map>
#include <optional>
#include "../Brushes/Core/BrushId.h"
#include "Position.h"

namespace MapEditor {
namespace Domain {

// Forward declaration
class ItemType;

// Position struct is now included directly

/**
 * Extended attributes for rare item properties.
 * Only allocated when needed (text, teleport, doors, depots).
 * Keeps common Item struct small.
 */
struct ExtendedAttributes {
    std::string text;              // Writable text (books, signs)
    std::string description;       // Custom description

// ...

    std::optional<Position> teleport_dest;  // Teleport destination (nullopt if not teleport)
    uint32_t depot_id = 0;         // Depot ID (0 if not depot)
    uint32_t door_id = 0;          // Door ID (0 if not door)
    
    // Generic attributes from OTBM_ATTR_ATTRIBUTE_MAP
    using AttributeValue = std::variant<std::string, int64_t, double, bool>;
    std::unordered_map<std::string, AttributeValue> generic_attributes;
    
    ~ExtendedAttributes() = default;
};

/**
 * Flat item data structure for common properties.
 * Replaces std::unordered_map<ItemAttribute, std::any> with direct members.
 * 
 * Memory layout: 16 bytes (packed)
 * - Old system: ~120 bytes (HashMap overhead + any allocations)
 * - New system: ~32 bytes (ItemData + optional ExtendedAttributes pointer)
 * - Savings: ~75% memory reduction
 */
struct ItemData {
    uint16_t action_id = 0;     // Action ID for scripting
    uint16_t unique_id = 0;     // Unique ID for scripting
    uint16_t count = 1;         // Stack count (for stackables)
    uint8_t charges = 0;        // Item charges (runes, etc.)
    uint8_t tier = 0;           // Item tier/upgrade level
    uint16_t duration = 0;      // Duration (lights, decaying items)
    
    // Flags for extended attributes (avoids nullptr checks)
    uint8_t has_text : 1 = 0;
    uint8_t has_description : 1 = 0;
    uint8_t has_teleport : 1 = 0;
    uint8_t has_depot : 1 = 0;
    uint8_t has_door : 1 = 0;
    uint8_t _padding : 3 = 0;       // Reserved for future flags
};

/**
 * Represents an item instance on a tile.
 * 
 * PERFORMANCE NOTES:
 * - Flat memory layout for cache efficiency
 * - Common attributes (action_id, unique_id, count) inline
 * - Rare attributes (text, teleport) in separate allocation
 * - Total size: 48 bytes (vs 120+ bytes with old std::any system)
 */
class Item {
public:
    Item() = default;
    explicit Item(uint16_t server_id, uint16_t subtype = 1);
    ~Item() = default;
    
    // Move semantics
    Item(Item&& other) noexcept = default;
    Item& operator=(Item&& other) noexcept = default;
    
    // Copy semantics
    Item(const Item& other);
    Item& operator=(const Item& other);
    
    // ========== Identifiers ==========
    
    uint16_t getServerId() const { return server_id_; }
    uint16_t getClientId() const { return client_id_; }
    void setServerId(uint16_t id) { server_id_ = id; }
    void setClientId(uint16_t id) { client_id_ = id; }
    
    // ========== Common Attributes (Inline, Fast Access) ==========
    
    uint16_t getActionId() const { return data_.action_id; }
    void setActionId(uint16_t id) { data_.action_id = id; }
    
    uint16_t getUniqueId() const { return data_.unique_id; }
    void setUniqueId(uint16_t id) { data_.unique_id = id; }
    
    uint16_t getCount() const { return data_.count; }
    void setCount(uint16_t count) { data_.count = count; }
    
    uint8_t getCharges() const { return data_.charges; }
    void setCharges(uint8_t charges) { data_.charges = charges; }
    
    uint8_t getTier() const { return data_.tier; }
    void setTier(uint8_t tier) { data_.tier = tier; }
    
    uint16_t getDuration() const { return data_.duration; }
    void setDuration(uint16_t duration) { data_.duration = duration; }
    
    // Subtype (legacy compatibility - maps to count for stackables)
    uint16_t getSubtype() const { return data_.count; }
    void setSubtype(uint16_t subtype) { data_.count = subtype; }
    
    // ========== Extended Attributes (Rare, Heap-Allocated) ==========
    
    const std::string& getText() const;
    void setText(const std::string& text);
    
    const std::string& getDescription() const;
    void setDescription(const std::string& desc);
    
    const Position* getTeleportDestination() const;
    void setTeleportDestination(const Position& dest);
    
    uint32_t getDepotId() const;
    void setDepotId(uint32_t id);
    
    uint32_t getDoorId() const;
    void setDoorId(uint32_t id);
    
    // Generic Attributes
    void setGenericAttribute(const std::string& key, const ExtendedAttributes::AttributeValue& value);
    const ExtendedAttributes::AttributeValue* getGenericAttribute(const std::string& key) const;
    
    // Check if extended attributes exist
    bool hasExtendedAttributes() const { return extended_ != nullptr; }
    
    // ========== Type Lookup ==========
    
    const ItemType* getType() const { return type_; }
    void setType(const ItemType* type) { type_ = type; }

    Brushes::BrushId getOwnerBrushId() const { return owner_brush_id_; }
    void setOwnerBrushId(Brushes::BrushId brushId) { owner_brush_id_ = brushId; }
    
    // ========== Container Support ==========
    
    void addContainerItem(std::unique_ptr<Item> item);
    const std::vector<std::unique_ptr<Item>>& getContainerItems() const { return container_items_; }
    bool isContainer() const { return !container_items_.empty(); }
    
    // ========== Utility ==========
    
    /**
     * Check if this item has any attributes that require full node serialization.
     * Items without attributes can use compact inline format in OTBM.
     * Complex = has actionId, uniqueId, count > 1, text, teleport, door, depot, container items
     */
    bool isComplex() const {
        // Inline attributes that make item complex
        if (data_.action_id > 0 || data_.unique_id > 0) return true;
        if (data_.count > 1) return true;  // Stackable with multiple items
        
        // Extended attributes
        if (extended_) return true;
        
        // Container with items
        if (!container_items_.empty()) return true;
        
        return false;
    }
    
    std::unique_ptr<Item> clone() const;

private:
    void ensureExtended();  // Allocate extended attributes if needed
    
    uint16_t server_id_ = 0;
    uint16_t client_id_ = 0;
    
    const ItemType* type_ = nullptr;
    
    // Flat inline data (16 bytes) - common case
    ItemData data_;
    
    // Extended attributes (heap-allocated only when needed) - rare case
    std::unique_ptr<ExtendedAttributes> extended_;
    
    // Container items
    std::vector<std::unique_ptr<Item>> container_items_;
    Brushes::BrushId owner_brush_id_ = Brushes::InvalidBrushId;
};

} // namespace Domain
} // namespace MapEditor
