#include "Item.h"
#include "ItemType.h"
#include "Position.h"
namespace MapEditor {
namespace Domain {

// ExtendedAttributes destructor is now defaulted (unique_ptr handles cleanup)

Item::Item(uint16_t server_id, uint16_t subtype)
    : server_id_(server_id)
{
    data_.count = subtype;
}

Item::Item(const Item& other)
    : server_id_(other.server_id_)
    , client_id_(other.client_id_)
    , type_(other.type_)
    , data_(other.data_)
{
    // Deep copy extended attributes if they exist
    if (other.extended_) {
        extended_ = std::make_unique<ExtendedAttributes>();
        extended_->text = other.extended_->text;
        extended_->description = other.extended_->description;
        extended_->depot_id = other.extended_->depot_id;
        extended_->door_id = other.extended_->door_id;
        extended_->generic_attributes = other.extended_->generic_attributes;
        
        // Value copy is handled by std::optional value semantics
        extended_->teleport_dest = other.extended_->teleport_dest;
    }
    
    // Deep copy container items
    container_items_.reserve(other.container_items_.size());
    for (const auto& item : other.container_items_) {
        if (item) {
            container_items_.push_back(item->clone());
        }
    }
}

Item& Item::operator=(const Item& other) {
    if (this != &other) {
        server_id_ = other.server_id_;
        client_id_ = other.client_id_;
        type_ = other.type_;
        data_ = other.data_;
        
        // Deep copy extended attributes
        if (other.extended_) {
            extended_ = std::make_unique<ExtendedAttributes>();
            extended_->text = other.extended_->text;
            extended_->description = other.extended_->description;
            extended_->depot_id = other.extended_->depot_id;
            extended_->door_id = other.extended_->door_id;
            extended_->generic_attributes = other.extended_->generic_attributes;
            
            // Value copy is handled by std::optional value semantics
            extended_->teleport_dest = other.extended_->teleport_dest;
        } else {
            extended_.reset();
        }
        
        // Deep copy container items
        container_items_.clear();
        container_items_.reserve(other.container_items_.size());
        for (const auto& item : other.container_items_) {
            if (item) {
                container_items_.push_back(item->clone());
            }
        }
    }
    return *this;
}

void Item::ensureExtended() {
    if (!extended_) {
        extended_ = std::make_unique<ExtendedAttributes>();
    }
}

// ========== Extended Attributes ==========

const std::string& Item::getText() const {
    static const std::string empty_string;
    return (extended_ && data_.has_text) ? extended_->text : empty_string;
}

void Item::setText(const std::string& text) {
    if (!text.empty()) {
        ensureExtended();
        data_.has_text = 1;
        extended_->text = text;
    } else if (extended_) {
        data_.has_text = 0;
        extended_->text.clear();
    }
}

const std::string& Item::getDescription() const {
    static const std::string empty_string;
    return (extended_ && data_.has_description) ? extended_->description : empty_string;
}

void Item::setDescription(const std::string& desc) {
    if (!desc.empty()) {
        ensureExtended();
        data_.has_description = 1;
        extended_->description = desc;
    } else if (extended_) {
        data_.has_description = 0;
        extended_->description.clear();
    }
}

const Position* Item::getTeleportDestination() const {
    return (extended_ && data_.has_teleport && extended_->teleport_dest.has_value()) ? &(*extended_->teleport_dest) : nullptr;
}

void Item::setTeleportDestination(const Position& dest) {
    ensureExtended();
    data_.has_teleport = 1;
    extended_->teleport_dest = dest;
}

uint32_t Item::getDepotId() const {
    return (extended_ && data_.has_depot) ? extended_->depot_id : 0;
}

void Item::setDepotId(uint32_t id) {
    if (id > 0) {
        ensureExtended();
        data_.has_depot = 1;
        extended_->depot_id = id;
    } else if (extended_) {
        data_.has_depot = 0;
        extended_->depot_id = 0;
    }
}

uint32_t Item::getDoorId() const {
    return (extended_ && data_.has_door) ? extended_->door_id : 0;
}

void Item::setDoorId(uint32_t id) {
    if (id > 0) {
        ensureExtended();
        data_.has_door = 1;
        extended_->door_id = id;
    } else if (extended_) {
        data_.has_door = 0;
        extended_->door_id = 0;
    }
}

void Item::setGenericAttribute(const std::string& key, const ExtendedAttributes::AttributeValue& value) {
    ensureExtended();
    extended_->generic_attributes[key] = value;
}

const ExtendedAttributes::AttributeValue* Item::getGenericAttribute(const std::string& key) const {
    if (!extended_) return nullptr;
    auto it = extended_->generic_attributes.find(key);
    if (it != extended_->generic_attributes.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Item::hasPodiumOutfit() const {
    return getGenericAttribute("podium_flags") != nullptr;
}

const std::unordered_map<std::string, ExtendedAttributes::AttributeValue>& Item::getGenericAttributes() const {
    static const std::unordered_map<std::string, ExtendedAttributes::AttributeValue> empty;
    return extended_ ? extended_->generic_attributes : empty;
}

// ========== Container Support ==========

void Item::addContainerItem(std::unique_ptr<Item> item) {
    if (item) {
        container_items_.push_back(std::move(item));
    }
}

// ========== Utility ==========

std::unique_ptr<Item> Item::clone() const {
    return std::make_unique<Item>(*this);
}

} // namespace Domain
} // namespace MapEditor
