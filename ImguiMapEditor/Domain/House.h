#pragma once
#include "Position.h"
#include <string>
#include <vector>
#include <cstdint>

namespace MapEditor {
namespace Domain {

struct House {
    uint32_t id = 0;
    std::string name;
    Position entry_position;
    uint32_t rent = 0;
    uint32_t town_id = 0;
    bool is_guildhall = false;
    std::vector<Position> tiles;

    House(uint32_t house_id) : id(house_id) {}
};

} // namespace Domain
} // namespace MapEditor
