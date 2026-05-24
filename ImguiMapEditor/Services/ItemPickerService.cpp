#include "ItemPickerService.h"
#include "Services/ClientDataService.h"
#include "Domain/ItemType.h"
#include <algorithm>
#include <cctype>

namespace MapEditor::AppLogic {

ItemPickerService::ItemPickerService(Services::ClientDataService* client_data)
    : client_data_(client_data) {
}

std::vector<Domain::Search::PickResult> ItemPickerService::search(
    const std::string& query, size_t limit) const {
    
    std::vector<Domain::Search::PickResult> results;
    if (!client_data_ || query.empty()) {
        return results;
    }
    
    std::string const query_lower = toLower(query);
    
    // Check for "cid:" prefix (client ID search)
    bool const is_client_id_search = query_lower.substr(0, 4) == "cid:";
    uint16_t search_id = 0;
    bool is_id_search = false;
    
    if (is_client_id_search && query.length() > 4) {
        try {
            search_id = static_cast<uint16_t>(std::stoi(query.substr(4)));
            is_id_search = true;
        } catch (...) { }
    } else {
        // Check if query is a number (server ID search)
        bool const all_digits = !query.empty() && 
            std::all_of(query.begin(), query.end(), ::isdigit);
        if (all_digits) {
            try {
                search_id = static_cast<uint16_t>(std::stoi(query));
                is_id_search = true;
            } catch (...) { }
        }
    }
    
    // Search items
    const auto& items = client_data_->getItemTypes();
    for (const auto& item : items) {
        if (results.size() >= limit) break;
        if (item.server_id == 0) continue;
        
        bool match = false;
        
        if (is_id_search) {
            if (is_client_id_search) {
                match = (item.client_id == search_id);
            } else {
                match = (item.server_id == search_id);
            }
        } else {
            // Fuzzy name search
            match = matchesFuzzy(item.name, query_lower);
        }
        
        if (match) {
            Domain::Search::PickResult result;
            result.server_id = item.server_id;
            result.name = item.name.empty() 
                ? "Item " + std::to_string(item.server_id)
                : item.name;
            result.is_creature = false;
            results.push_back(result);
        }
    }
    
    // Search creatures by name
    if (!is_id_search) {  // Creatures don't have server/client IDs
        const auto& creature_map = client_data_->getCreatureMap();
        for (const auto& [name, creature] : creature_map) {
            if (results.size() >= limit) break;
            if (!creature) continue;
            
            if (matchesFuzzy(name, query_lower)) {
                Domain::Search::PickResult result;
                result.server_id = 0;  // Creatures don't have server ID
                result.name = creature->name;
                result.is_creature = true;
                results.push_back(result);
            }
        }
    }
    
    // Sort by name for consistent display
    std::sort(results.begin(), results.end());
    
    return results;
}

bool ItemPickerService::matchesFuzzy(const std::string& text, 
                                      const std::string& query) const {
    std::string const text_lower = toLower(text);
    
    // Simple substring match for now
    // Can be upgraded to fuzzy matching algorithm later
    return text_lower.find(query) != std::string::npos;
}

std::string ItemPickerService::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace MapEditor::AppLogic
