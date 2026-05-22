#include "TileSnapshot.h"
#include "../Item.h"
#include "../Spawn.h"
#include "../Creature.h"
#include <cstring>

namespace MapEditor::Domain::History {

// Binary serialization format:
// [has_data: 1 byte]
// [Position: 10 bytes (x:4, y:4, z:2)]
// [flags: 2 bytes]
// [house_id: 4 bytes]
// [optional_border: 1 byte]
// [has_ground: 1 byte]
// [ground_server_id: 2 bytes] (if has_ground)
// [ground_client_id: 2 bytes] (if has_ground)
// [item_count: 2 bytes]
// [items: variable]
// [has_spawn: 1 byte]
// [spawn_data: variable] (if has_spawn)
// [has_creature: 1 byte]
// [creature_data: variable] (if has_creature)

namespace {
// Helper to write primitive types
template<typename T>
void write(std::vector<uint8_t>& buf, const T& value) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), ptr, ptr + sizeof(T));
}

// Helper to read primitive types
template<typename T>
T read(const uint8_t*& ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

void writeString(std::vector<uint8_t>& buf, const std::string& str) {
    uint16_t len = static_cast<uint16_t>(str.length());
    write(buf, len);
    buf.insert(buf.end(), str.begin(), str.end());
}

std::string readString(const uint8_t*& ptr) {
    uint16_t len = read<uint16_t>(ptr);
    std::string str(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return str;
}

void serializeItem(std::vector<uint8_t>& buf, const Item* item) {
    // Core IDs
    write(buf, item->getServerId());
    write(buf, item->getClientId());
    write(buf, item->getOwnerBrushId());
    
    // ItemData inline properties
    write(buf, item->getActionId());
    write(buf, item->getUniqueId());
    write(buf, item->getCount());
    write(buf, item->getCharges());
    write(buf, item->getTier());
    write(buf, item->getDuration());
    
    // Extended attribute flags (what optional data follows)
    uint8_t flags = 0;
    std::string text = item->getText();
    std::string desc = item->getDescription();
    const Position* teleport = item->getTeleportDestination();
    uint32_t depotId = item->getDepotId();
    uint32_t doorId = item->getDoorId();
    
    if (!text.empty()) flags |= 0x01;
    if (!desc.empty()) flags |= 0x02;
    if (teleport) flags |= 0x04;
    if (depotId != 0) flags |= 0x08;
    if (doorId != 0) flags |= 0x10;
    if (item->isContainer() && !item->getContainerItems().empty()) flags |= 0x20;
    
    write(buf, flags);
    
    // Write optional extended data
    if (flags & 0x01) writeString(buf, text);
    if (flags & 0x02) writeString(buf, desc);
    if (flags & 0x04) {
        write(buf, static_cast<int32_t>(teleport->x));
        write(buf, static_cast<int32_t>(teleport->y));
        write(buf, teleport->z);
    }
    if (flags & 0x08) write(buf, depotId);
    if (flags & 0x10) write(buf, doorId);
    
    // Container items (recursive)
    if (flags & 0x20) {
        const auto& containerItems = item->getContainerItems();
        write(buf, static_cast<uint16_t>(containerItems.size()));
        for (const auto& child : containerItems) {
            serializeItem(buf, child.get());
        }
    }
}

std::unique_ptr<Item> deserializeItem(const uint8_t*& ptr) {
    // Core IDs
    uint16_t server_id = read<uint16_t>(ptr);
    uint16_t client_id = read<uint16_t>(ptr);
    auto owner_brush_id = read<Brushes::BrushId>(ptr);
    auto item = std::make_unique<Item>(server_id);
    item->setClientId(client_id);
    item->setOwnerBrushId(owner_brush_id);
    
    // ItemData inline properties
    item->setActionId(read<uint16_t>(ptr));
    item->setUniqueId(read<uint16_t>(ptr));
    item->setCount(read<uint16_t>(ptr));
    item->setCharges(read<uint8_t>(ptr));
    item->setTier(read<uint8_t>(ptr));
    item->setDuration(read<uint16_t>(ptr));
    
    // Extended attribute flags
    uint8_t flags = read<uint8_t>(ptr);
    
    // Read optional extended data
    if (flags & 0x01) item->setText(readString(ptr));
    if (flags & 0x02) item->setDescription(readString(ptr));
    if (flags & 0x04) {
        Position dest;
        dest.x = read<int32_t>(ptr);
        dest.y = read<int32_t>(ptr);
        dest.z = read<int16_t>(ptr);
        item->setTeleportDestination(dest);
    }
    if (flags & 0x08) item->setDepotId(read<uint32_t>(ptr));
    if (flags & 0x10) item->setDoorId(read<uint32_t>(ptr));
    
    // Container items (recursive)
    if (flags & 0x20) {
        uint16_t count = read<uint16_t>(ptr);
        for (uint16_t i = 0; i < count; ++i) {
            item->addContainerItem(deserializeItem(ptr));
        }
    }
    
    return item;
}

} // anonymous namespace

TileSnapshot TileSnapshot::capture(const Tile* tile, const Position& pos) {
    TileSnapshot snapshot;
    snapshot.position_ = pos;
    
    if (!tile) {
        // Empty tile - no data needed
        return snapshot;
    }
    
    snapshot.serializeTile(tile);
    return snapshot;
}

void TileSnapshot::serializeTile(const Tile* tile) {
    data_.clear();
    data_.reserve(256);  // Pre-allocate reasonable size
    
    // Marker that we have data
    write(data_, static_cast<uint8_t>(1));
    
    // Position
    write(data_, position_.x);
    write(data_, position_.y);
    write(data_, position_.z);
    
    // Flags
    write(data_, static_cast<uint16_t>(tile->getFlags()));
    
    // House ID
    write(data_, tile->getHouseId());
    write(data_, tile->getGroundBrushId());
    write(data_, tile->getOptionalBorderBrushId());
    write(data_, tile->getSpawnBrushId());
    write(data_, tile->getCreatureBrushId());
    write(data_, tile->getHouseBrushId());
    write(data_, tile->getHouseExitBrushId());
    write(data_, tile->getHouseExitHouseId());
    write(data_, tile->getWaypointBrushId());
    write(data_, tile->getZoneBrushId(TileFlag::ProtectionZone));
    write(data_, tile->getZoneBrushId(TileFlag::NoPvp));
    write(data_, tile->getZoneBrushId(TileFlag::NoLogout));
    write(data_, tile->getZoneBrushId(TileFlag::PvpZone));
    write(data_, tile->getZoneBrushId(TileFlag::Refresh));

    // Optional border marker
    write(data_, static_cast<uint8_t>(tile->hasOptionalBorder() ? 1 : 0));
    
    // Ground item
    const Item* ground = tile->getGround();
    write(data_, static_cast<uint8_t>(ground ? 1 : 0));
    if (ground) {
        serializeItem(data_, ground);
    }
    
    // Stacked items
    const auto& items = tile->getItems();
    write(data_, static_cast<uint16_t>(items.size()));
    for (const auto& item : items) {
        serializeItem(data_, item.get());
    }
    
    // Spawn
    const Spawn* spawn = tile->getSpawn();
    write(data_, static_cast<uint8_t>(spawn ? 1 : 0));
    if (spawn) {
        write(data_, spawn->radius);  // Use public member
    }
    
    // Creature
    const Creature* creature = tile->getCreature();
    write(data_, static_cast<uint8_t>(creature ? 1 : 0));
    if (creature) {
        writeString(data_, creature->name);  // Use public member
    }
}

std::unique_ptr<Tile> TileSnapshot::restore() const {
    return deserializeTile();
}

std::unique_ptr<Tile> TileSnapshot::deserializeTile() const {
    if (data_.empty()) {
        return nullptr;  // Empty tile
    }
    
    const uint8_t* ptr = data_.data();
    
    // Check marker
    uint8_t has_data = read<uint8_t>(ptr);
    if (!has_data) {
        return nullptr;
    }
    
    // Position
    int32_t x = read<int32_t>(ptr);
    int32_t y = read<int32_t>(ptr);
    int16_t z = read<int16_t>(ptr);
    Position pos{x, y, z};
    
    auto tile = std::make_unique<Tile>(pos);
    
    // Flags
    uint16_t flags = read<uint16_t>(ptr);
    tile->setFlags(flags);
    
    // House ID
    uint32_t house_id = read<uint32_t>(ptr);
    tile->setHouseId(house_id);
    tile->setGroundBrushId(read<Brushes::BrushId>(ptr));
    tile->setOptionalBorderBrushId(read<Brushes::BrushId>(ptr));
    tile->setSpawnBrushId(read<Brushes::BrushId>(ptr));
    tile->setCreatureBrushId(read<Brushes::BrushId>(ptr));
    tile->setHouseBrushId(read<Brushes::BrushId>(ptr));
    tile->setHouseExitBrushId(read<Brushes::BrushId>(ptr));
    tile->setHouseExitHouseId(read<uint32_t>(ptr));
    tile->setWaypointBrushId(read<Brushes::BrushId>(ptr));
    tile->setZoneBrushId(TileFlag::ProtectionZone, read<Brushes::BrushId>(ptr));
    tile->setZoneBrushId(TileFlag::NoPvp, read<Brushes::BrushId>(ptr));
    tile->setZoneBrushId(TileFlag::NoLogout, read<Brushes::BrushId>(ptr));
    tile->setZoneBrushId(TileFlag::PvpZone, read<Brushes::BrushId>(ptr));
    tile->setZoneBrushId(TileFlag::Refresh, read<Brushes::BrushId>(ptr));

    // Optional border
    uint8_t has_optional_border = read<uint8_t>(ptr);
    tile->setOptionalBorder(has_optional_border != 0);
    
    // Ground item
    uint8_t has_ground = read<uint8_t>(ptr);
    if (has_ground) {
        tile->setGround(deserializeItem(ptr));
    }
    
    // Stacked items - use addItemDirect to preserve exact order (no sorting)
    uint16_t item_count = read<uint16_t>(ptr);
    for (uint16_t i = 0; i < item_count; ++i) {
        auto item = deserializeItem(ptr);
        tile->addItemDirect(std::move(item));  // Direct append, no sorting
    }
    
    // Spawn
    uint8_t has_spawn = read<uint8_t>(ptr);
    if (has_spawn) {
        int32_t radius = read<int32_t>(ptr);
        auto spawn = std::make_unique<Spawn>(pos, radius);
        tile->setSpawn(std::move(spawn));
    }
    
    // Creature
    uint8_t has_creature = read<uint8_t>(ptr);
    if (has_creature) {
        std::string name = readString(ptr);
        auto creature = std::make_unique<Creature>(name);
        tile->setCreature(std::move(creature));
    }
    
    return tile;
}

size_t TileSnapshot::memsize() const {
    return sizeof(*this) + data_.capacity();
}

} // namespace MapEditor::Domain::History
