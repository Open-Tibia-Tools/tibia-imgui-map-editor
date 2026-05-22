#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "Core/Config.h"

namespace MapEditor {
namespace IO {

/**
 * Represents a sprite loaded from SPR file
 * Contains compressed pixel data
 */
struct SpriteData {
    uint32_t id = 0;
    uint16_t compressed_size = 0;
    std::vector<uint8_t> compressed_pixels;
    bool is_empty = true;
    
    // Decoded RGBA data (populated lazily)
    std::vector<uint8_t> rgba_data;

    // [UB FIX] Use atomic for thread-safe double-checked locking
    std::atomic<bool> is_decoded{false};
    
    /**
     * Decode the sprite to RGBA format
     * @param use_transparency If true, use magenta as transparent
     * @return true if decoding successful
     */
    bool decode(bool use_transparency = true);
    
    static constexpr uint32_t SPRITE_SIZE = Config::Rendering::SPRITE_SIZE;
    static constexpr uint32_t SPRITE_PIXELS = SPRITE_SIZE * SPRITE_SIZE;
    static constexpr uint32_t RGBA_SIZE = SPRITE_PIXELS * 4;

private:
    std::mutex mutex_;
};

/**
 * Result of SPR parsing
 */
struct SprResult {
    bool success = false;
    uint32_t signature = 0;
    uint32_t sprite_count = 0;
    std::string error;
};

/**
 * Reads Tibia .spr sprite files
 * Provides lazy loading of individual sprites
 */
class SprReader {
public:
    SprReader() = default;
    ~SprReader() = default;
    
    // No copy (file handle)
    SprReader(const SprReader&) = delete;
    SprReader& operator=(const SprReader&) = delete;
    
    // Move disabled: std::mutex makes the reader non-movable.
    SprReader(SprReader&&) = delete;
    SprReader& operator=(SprReader&&) = delete;
    
    /**
     * Open a .spr file
     * @param path Path to Tibia.spr
     * @param expected_signature Expected signature for validation (0 to skip)
     * @param extended True for extended sprite IDs (Tibia 9.60+)
     * @return Result with sprite count and validation
     */
    SprResult open(const std::filesystem::path& path, 
                   uint32_t expected_signature = 0,
                   bool extended = false);
    
    /**
     * Load a specific sprite
     * @param sprite_id Sprite ID (1-based, 0 is blank)
     * @return Sprite data or nullptr if not found
     */
    std::shared_ptr<SpriteData> loadSprite(uint32_t sprite_id);
    
    /**
     * Get sprite count
     */
    uint32_t getSpriteCount() const {
        std::lock_guard lock(mutex_);
        return sprite_count_;
    }
    
    /**
     * Get file signature
     */
    uint32_t getSignature() const {
        std::lock_guard lock(mutex_);
        return signature_;
    }
    
    /**
     * Check if file is open
     */
    bool isOpen() const {
        std::lock_guard lock(mutex_);
        return file_.is_open();
    }

private:
    mutable std::mutex mutex_;  // Protects file_ and cache_ for thread-safety
    std::ifstream file_;
    uint32_t signature_ = 0;
    uint32_t sprite_count_ = 0;
    bool extended_ = false;
    std::vector<uint32_t> offsets_;
    
    // Cache of loaded sprites
    std::unordered_map<uint32_t, std::shared_ptr<SpriteData>> cache_;
};

} // namespace IO
} // namespace MapEditor
