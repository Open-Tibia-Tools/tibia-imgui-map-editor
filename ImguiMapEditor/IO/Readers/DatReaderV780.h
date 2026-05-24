#pragma once
#include "DatReaderBase.h"
namespace MapEditor {
namespace IO {

/**
 * DAT Reader for Tibia 7.80 - 8.54
 * - patternZ now read from file
 * - Adds: GROUND_BORDER, DONT_HIDE, IGNORE_LOOK
 */
class DatReaderV780 : public DatReaderBase {
public:
    std::pair<uint32_t, uint32_t> getVersionRange() const override {
        return {780, 854};
    }
    
    const char* getName() const override {
        return "DatReaderV780 (Tibia 7.80-8.54)";
    }

protected:
    uint8_t transformFlag(uint8_t raw) override;
    
};

} // namespace IO
} // namespace MapEditor
