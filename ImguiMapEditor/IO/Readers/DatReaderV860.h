#pragma once
#include "DatReaderBase.h"
namespace MapEditor {
namespace IO {

/**
 * DAT Reader for Tibia 8.60 - 9.86
 * - Adds: TRANSLUCENT, CLOTH, MARKET_ITEM
 */
class DatReaderV860 : public DatReaderBase {
public:
    std::pair<uint32_t, uint32_t> getVersionRange() const override {
        return {860, 986};
    }
    
    const char* getName() const override {
        return "DatReaderV860 (Tibia 8.60-9.86)";
    }

protected:
    // No overrides needed for readItemFlags/transformFlag as this is the canonical version
    
    bool usesExtendedSprites() const override { return false; }
};

} // namespace IO
} // namespace MapEditor
