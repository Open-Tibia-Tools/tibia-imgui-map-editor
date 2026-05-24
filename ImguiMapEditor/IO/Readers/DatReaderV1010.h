#pragma once
#include "DatReaderBase.h"
namespace MapEditor {
namespace IO {

/**
 * DAT Reader for Tibia 10.10+
 * - Adds: NO_MOVE_ANIMATION, DEFAULT_ACTION, WRAPPABLE, UNWRAPPABLE, TOP_EFFECT
 * - Uses extended 32-bit sprite IDs (10.50+)
 * - Has frame duration data (10.50+)
 * - Has frame groups for outfits (10.57+)
 */
class DatReaderV1010 : public DatReaderBase {
public:
    explicit DatReaderV1010(uint32_t version = 1010) : version_(version) {}
    
    std::pair<uint32_t, uint32_t> getVersionRange() const override {
        return {1010, 9999};  // All future versions
    }
    
    const char* getName() const override {
        return "DatReaderV1010 (Tibia 10.10+)";
    }

protected:
    uint8_t transformFlag(uint8_t raw) override;
    
    bool usesExtendedSprites() const override { return version_ >= 960; }
    bool hasFrameDurations() const override { return version_ >= 1050; }
    bool hasFrameGroups() const override { return version_ >= 1057; }

private:
    uint32_t version_;
};

} // namespace IO
} // namespace MapEditor
