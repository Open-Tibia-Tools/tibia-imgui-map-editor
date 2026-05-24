#pragma once
#include "DatReaderBase.h"
namespace MapEditor {
namespace IO {

/**
 * DAT Reader for Tibia 7.55 - 7.72
 * - Offset values now read from file
 * - patternZ now read from file (unlike 7.40 and earlier)
 */
class DatReaderV755 : public DatReaderBase {
public:
    std::pair<uint32_t, uint32_t> getVersionRange() const override {
        return {755, 772};
    }
    
    const char* getName() const override {
        return "DatReaderV755 (Tibia 7.55-7.72)";
    }

protected:
    uint8_t transformFlag(uint8_t raw) override;
    
    // V755+ reads patternZ from file
};

} // namespace IO
} // namespace MapEditor
