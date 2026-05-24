#pragma once
#include "Domain/ClientVersionTypes.h"
#include "Domain/ItemType.h"
#include "Domain/CreatureType.h"
#include "IO/OtbReader.h"
#include "IO/SrvReader.h"
#include "IO/SprReader.h"
#include "IO/Readers/DatReaderFactory.h"
#include "IO/CreatureXmlReader.h"
#include "IO/ItemXmlReader.h"
// NOTE: TilesetXmlReader include moved to TilesetService
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace MapEditor {
namespace Services {

// Forward declaration
class SpriteManager;

/**
 * Result of client data loading
 */
struct ClientDataResult {
    bool success = false;
    std::string error;
    
    // Version info
    IO::OtbVersionInfo otb_version;
    uint32_t dat_signature = 0;
    uint32_t spr_signature = 0;
    
    // Statistics
    size_t item_count = 0;
    size_t outfit_count = 0;
    size_t effect_count = 0;
    size_t missile_count = 0;
    size_t sprite_count = 0;
    size_t creature_count = 0; // [NEW]
};

/**
 * Progress callback for loading operations
 */
using LoadProgressCallback = std::function<void(int percent, const std::string& status)>;

/**
 * Orchestrates loading of client data files (OTB, DAT, SPR, JSON).
 * Manages the item type database with server ID → client ID mapping.
 */
class ClientDataService {
public:
    ClientDataService() = default;
    ~ClientDataService() = default;
    
    // Non-copyable
    ClientDataService(const ClientDataService&) = delete;
    ClientDataService& operator=(const ClientDataService&) = delete;
    
    /**
     * Load all client data for a specific version
     * @param client_path Path to Tibia client directory (containing Tibia.dat, Tibia.spr)
     * @param item_metadata_path Path to items.otb or items.srv file
     * @param client_version Client version number (e.g., 860, 1010)
     * @param data_source The item configuration type (OTB, SRV, or DAT-only)
     * @param progress Optional progress callback
     * @return Result with load status and statistics
     */
    ClientDataResult load(const std::filesystem::path& client_path,
                          const std::filesystem::path& item_metadata_path,
                          uint32_t client_version,
                          ::MapEditor::Domain::ItemDataSource data_source = ::MapEditor::Domain::ItemDataSource::OTB,
                          LoadProgressCallback progress = nullptr);
    
    /**
     * Load creature data from creatures.xml.
     */
    bool loadCreatureData(const std::filesystem::path& creatures_xml_path);

    /**
     * Load item game attributes from items.xml.
     * Must be called after load() - augments existing ItemType data.
     * @param items_xml_path Path to items.xml
     * @return true if loaded successfully
     */
    bool loadItemData(const std::filesystem::path& items_xml_path);

    // NOTE: loadTilesetData moved to TilesetService

    /**
     * Check if data is loaded
     */
    bool isLoaded() const { return loaded_; }
    
    /**
     * Get loaded client version
     */
    uint32_t getClientVersion() const { return client_version_; }
    
    /**
     * Check if server ID support is available (loaded from items.srv)
     * Required for SEC map loading.
     */
    bool hasServerIdSupport() const { return !server_id_index_.empty(); }
    
    /**
     * Get item type by server ID
     * @return nullptr if not found
     */
    const Domain::ItemType* getItemTypeByServerId(uint16_t server_id) const;
    
    /**
     * Get item type by client ID
     * @return nullptr if not found
     */
    const Domain::ItemType* getItemTypeByClientId(uint16_t client_id) const;
    
    /**
     * Get creature type by name (case insensitive)
     * @return nullptr if not found
     */
    const Domain::CreatureType* getCreatureType(const std::string& name) const;

    /**
     * Get outfit data by lookType for creature sprite rendering.
     * Returns outfit/creature appearance data from DAT file.
     * @param lookType Outfit lookType ID from creatures.xml
     * @return ClientItem with sprite dimensions and IDs, or nullptr if not found
     */
    const IO::ClientItem* getOutfitData(uint16_t lookType) const;
    
    /**
     * Get outfit sprite IDs by lookType.
     * @param lookType Outfit lookType ID
     * @return Vector of sprite IDs, empty if not found
     */
    std::vector<uint32_t> getOutfitSpriteIds(uint16_t lookType) const;

    /**
     * Get sprite reader for texture loading
     */
    std::shared_ptr<IO::SprReader> getSpriteReader() { return spr_reader_; }
    const std::shared_ptr<IO::SprReader> getSpriteReader() const { return spr_reader_; }
    
    /**
     * Get all item types (const)
     */
    const std::vector<Domain::ItemType>& getItemTypes() const { return items_; }
    
    /**
     * Get all item types (mutable - for sprite caching optimization)
     */
    std::vector<Domain::ItemType>& getItemTypes() { return items_; }
    
    /**
     * Get maximum server ID
     */
    uint16_t getMaxServerId() const { return max_server_id_; }
    
    /**
     * Get maximum client ID
     */
    uint16_t getMaxClientId() const { return max_client_id_; }
    
    /**
     * Get all creatures (for search functionality)
     */
    const std::vector<std::unique_ptr<Domain::CreatureType>>& getCreatures() const { return creatures_; }
    
    /**
     * Get creature lookup map (name → CreatureType*)
     */
    const std::unordered_map<std::string, Domain::CreatureType*>& getCreatureMap() const { return creature_map_; }
    
    /**
     * PERFORMANCE: Optimize ItemType data structure by caching sprite region pointers.
     * Eliminates frequent hash lookups for simple items.
     *
     * @param sprite_manager SpriteManager to load sprites from
     * @param preload_sprites If true, forces loading of sprites to atlas
     * @return Number of items optimized
     */
    size_t optimizeItemSprites(SpriteManager& sprite_manager, bool preload_sprites = true);

    /**
     * Clear all loaded data
     */
    void clear();

private:
    void mergeOtbWithDat(const std::vector<Domain::ItemType>& otb_items,
                         const IO::DatResult& dat_result,
                         uint32_t client_version);
    
    // NOTE: generateCreatureTileset moved to TilesetService

    bool loaded_ = false;
    uint32_t client_version_ = 0;
    uint16_t max_server_id_ = 0;
    uint16_t max_client_id_ = 0;
    
    // Item type storage
    std::vector<Domain::ItemType> items_;
    std::unordered_map<uint16_t, size_t> server_id_index_;  // server_id → items_ index
    std::unordered_map<uint16_t, size_t> client_id_index_;  // client_id → items_ index
    
    // Creature storage
    std::vector<std::unique_ptr<Domain::CreatureType>> creatures_;
    std::unordered_map<std::string, Domain::CreatureType*> creature_map_;
    
    // Outfit storage (from DAT for creature sprite lookup)
    std::vector<IO::ClientItem> outfits_;
    std::unordered_map<uint16_t, size_t> outfit_index_;  // lookType → outfits_ index

    // Sprite reader (for lazy sprite loading)
    std::shared_ptr<IO::SprReader> spr_reader_;
};

} // namespace Services
} // namespace MapEditor
