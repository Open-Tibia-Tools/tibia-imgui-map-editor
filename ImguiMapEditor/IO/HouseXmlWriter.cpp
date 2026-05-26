#include "HouseXmlWriter.h"
#include <fstream>
#include <sstream>

namespace MapEditor::IO {

bool HouseXmlWriter::write(
    const std::filesystem::path& path,
    const Domain::ChunkedMap& map
) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<houses>\n";
    
    const auto& houses = map.getHouses();
    for (const auto& [id, house] : houses) {
        if (!house) continue;
        
        file << "\t<house houseid=\"" << house->id << "\"";
        file << " name=\"" << house->name << "\"";
        file << " entryx=\"" << house->entry_position.x << "\"";
        file << " entryy=\"" << house->entry_position.y << "\"";
        file << " entryz=\"" << static_cast<int>(house->entry_position.z) << "\"";
        file << " rent=\"" << house->rent << "\"";
        file << " townid=\"" << house->town_id << "\"";
        file << " size=\"" << house->tiles.size() << "\"";
        if (house->is_guildhall) {
            file << " guildhall=\"true\"";
        }
        file << "/>\n";
    }
    
    file << "</houses>\n";
    file.close();
    
    return true;
}

} // namespace MapEditor::IO
