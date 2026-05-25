#pragma once
#include "../Position.h"
#include <string>

namespace MapEditor::Domain::Search {

/**
 * Result from a map search (items/creatures found on the map)
 */
struct MapSearchResult {
    Position position;
    uint16_t item_id = 0;           // 0 if creature
    std::string creature_name;      // empty if item
    std::string display_name;       // formatted name for display
    bool is_in_container = false;   // true if found inside a container
    
    bool isCreature() const { return item_id == 0 && !creature_name.empty(); }
    bool isItem() const { return item_id != 0; }
    
    // Format: "Name (ID) @ x,y,z"
    std::string getDisplayString() const {
        std::string result = display_name;
        if (item_id != 0) {
            result += " (" + std::to_string(item_id) + ")";
        }
        result += " @ " + std::to_string(position.x) + "," +
                  std::to_string(position.y) + "," +
                  std::to_string(position.z);
        return result;
    }
};

} // namespace MapEditor::Domain::Search
