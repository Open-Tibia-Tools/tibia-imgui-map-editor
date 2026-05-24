#include "PropertyPanelRenderer.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Spawn.h"
#include "Domain/Creature.h"
#include "Domain/Position.h"
#include "Domain/ChunkedMap.h"
#include "Services/SpriteManager.h"
#include "Rendering/Core/Texture.h"
#include "../../ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>
#include <cstring>
#include <cmath>
#include <string>

namespace MapEditor::UI::Properties {

void PropertyPanelRenderer::setContext(
    Domain::Item* item, Domain::Spawn* spawn, Domain::Creature* creature,
    uint32_t otbm_version, Services::SpriteManager* sprite_manager,
    uint16_t map_width, uint16_t map_height, Domain::ChunkedMap* map) 
{
    bool const context_changed = (item_ != item || spawn_ != spawn || creature_ != creature);
    
    item_ = item;
    spawn_ = spawn;
    creature_ = creature;
    otbm_version_ = otbm_version;
    sprite_manager_ = sprite_manager;
    map_width_ = map_width;
    map_height_ = map_height;
    map_ = map;
    
    if (item_) {
        item_type_ = item_->getType();
    } else {
        item_type_ = nullptr;
    }
    
    panel_type_ = detectPanelType();
    
    if (context_changed) {
        dirty_ = false;
        loadValuesFromContext();
        last_item_ = item_;
        last_spawn_ = spawn_;
        last_creature_ = creature_;
    }
}

PanelType PropertyPanelRenderer::detectPanelType() const {
    // Priority: Spawn > Creature > Item types
    if (spawn_ && !item_ && !creature_) return PanelType::Spawn;
    if (creature_ && !item_) return PanelType::Creature;
    if (!item_ || !item_type_) return PanelType::None;
    
    // Depot check FIRST - check if item has depot_id set (depots are also containers)
    // Note: Depot type is typically set via items.xml, not OTB group
    if (item_->getDepotId() > 0) return PanelType::Depot;
    
    // Container check - uses group from OTB
    if (item_type_->isContainer() || item_->isContainer()) return PanelType::Container;
    
    // Writeable check - uses group from OTB, also check maxTextLen, Readable flag, or if text is set
    if (item_type_->isWriteable() || 
        item_type_->maxTextLen > 0 || 
        item_type_->hasFlag(Domain::ItemFlag::Readable) ||
        !item_->getText().empty()) {
        return PanelType::Writeable;
    }
    
    // Fluid/Splash check - uses group from OTB  
    if (item_type_->isSplash() || item_type_->isFluidContainer()) return PanelType::Splash;
    
    // Door check - uses group from OTB
    if (item_type_->isDoor()) return PanelType::Door;
    
    // Teleport check - uses group from OTB
    if (item_type_->isTeleport()) return PanelType::Teleport;
    
    // Podium check - uses group from OTB
    if (item_type_->isPodium()) return PanelType::Podium;
    
    return PanelType::NormalItem;
}

const char* PropertyPanelRenderer::getPanelName() const {
    switch (panel_type_) {
        case PanelType::Container: return "Container";
        case PanelType::Writeable: return "Text";
        case PanelType::Splash: return "Fluid";
        case PanelType::Depot: return "Depot";
        case PanelType::Door: return "Door";
        case PanelType::Teleport: return "Teleport";
        case PanelType::Podium: return "Podium";
        case PanelType::NormalItem: return "Item";
        case PanelType::Spawn: return "Spawn";
        case PanelType::Creature: return "Creature";
        default: return "Properties";
    }
}

void PropertyPanelRenderer::loadValuesFromContext() {
    std::memset(&edit_, 0, sizeof(edit_));
    edit_.count = 1;
    edit_.spawn_radius = 1;
    edit_.spawn_time = 60;
    edit_.show_outfit = true;
    edit_.show_mount = true;
    edit_.show_platform = true;
    
    if (item_) {
        edit_.action_id = item_->getActionId();
        edit_.unique_id = item_->getUniqueId();
        edit_.count = item_->getCount();
        edit_.tier = item_->getTier();
        edit_.charges = item_->getCharges();
        edit_.door_id = static_cast<int>(item_->getDoorId());
        edit_.depot_id = static_cast<int>(item_->getDepotId());
        
        // Fluid subtype
        if (item_type_ && (item_type_->isSplash() || item_type_->isFluidContainer())) {
            edit_.fluid_type = item_->getSubtype();
        }
        
        // Teleport destination
        const Domain::Position* dest = item_->getTeleportDestination();
        if (dest) {
            edit_.tele_x = dest->x;
            edit_.tele_y = dest->y;
            edit_.tele_z = dest->z;
        }
        
        // Text
        std::string const text = item_->getText();
        if (!text.empty()) {
            std::strncpy(edit_.text, text.c_str(), sizeof(edit_.text) - 1);
        }
    }
    
    if (spawn_) {
        edit_.spawn_radius = spawn_->radius;
    }
    
    if (creature_) {
        edit_.spawn_time = creature_->spawn_time;
        edit_.direction = creature_->direction;
    }
}

void PropertyPanelRenderer::applyChangesToContext() {
    if (item_) {
        item_->setActionId(static_cast<uint16_t>(edit_.action_id));
        item_->setUniqueId(static_cast<uint16_t>(edit_.unique_id));
        item_->setCount(static_cast<uint16_t>(edit_.count));
        
        if (otbm_version_ >= 4) {
            item_->setTier(static_cast<uint8_t>(edit_.tier));
        }
        
        item_->setCharges(static_cast<uint8_t>(edit_.charges));
        item_->setDoorId(static_cast<uint32_t>(edit_.door_id));
        item_->setDepotId(static_cast<uint32_t>(edit_.depot_id));
        
        // Fluid type
        if (item_type_ && (item_type_->isSplash() || item_type_->isFluidContainer())) {
            item_->setSubtype(static_cast<uint16_t>(edit_.fluid_type));
        }
        
        // Teleport
        if (item_type_ && item_type_->isTeleport()) {
            Domain::Position const dest{edit_.tele_x, edit_.tele_y, static_cast<int16_t>(edit_.tele_z)};
            item_->setTeleportDestination(dest);
        }
        
        // Text - save only for writeable items
        if (panel_type_ == PanelType::Writeable) {
            item_->setText(edit_.text);
        }
    }
    
    if (spawn_) {
        spawn_->radius = edit_.spawn_radius;
    }
    
    if (creature_) {
        creature_->spawn_time = edit_.spawn_time;
        creature_->direction = edit_.direction;
    }
    
    apply_flash_frames_ = 15;  // Flash for ~0.25s at 60fps
}

void PropertyPanelRenderer::render() {
    if (panel_type_ == PanelType::None) {
        ImGui::TextDisabled("Select an item to view properties");
        return;
    }
    
    bool const was_dirty = dirty_;
    
    switch (panel_type_) {
        case PanelType::Container: renderContainerSection(); break;
        case PanelType::Writeable: renderWriteableSection(); break;
        case PanelType::Splash: renderSplashSection(); break;
        case PanelType::Depot: renderDepotSection(); break;
        case PanelType::Door: renderDoorSection(); break;
        case PanelType::Teleport: renderTeleportSection(); break;
        case PanelType::Podium: renderPodiumSection(); break;
        case PanelType::NormalItem: renderNormalSection(); break;
        case PanelType::Spawn: renderSpawnSection(); break;
        case PanelType::Creature: renderCreatureSection(); break;
        default: break;
    }
    
    // Auto-apply on change
    if (dirty_ && !was_dirty) {
        applyChangesToContext();
        dirty_ = false;
    }
    
    renderApplyIndicator();
}

void PropertyPanelRenderer::renderApplyIndicator() {
    if (apply_flash_frames_ > 0) {
        // Green border flash effect
        float const alpha = static_cast<float>(apply_flash_frames_) / 15.0f;
        ImU32 const color = IM_COL32(100, 255, 100, static_cast<int>(200 * alpha));
        
        ImVec2 const min = ImGui::GetWindowPos();
        ImVec2 const max = ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddRect(min, max, color, 0, 0, 2.0f);
        
        apply_flash_frames_--;
    }
}

void PropertyPanelRenderer::renderCommonItemFields() {
    if (!item_ || !item_type_) return;
    
    // Item ID display
    ImGui::Text("ID: %u", item_->getServerId());
    if (!item_type_->name.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", item_type_->name.c_str());
    }
    
    ImGui::Separator();
    
    dirty_ |= PropertyWidgets::inputActionId(edit_.action_id);
    dirty_ |= PropertyWidgets::inputUniqueId(edit_.unique_id);
    
    // Count for stackables
    if (item_type_->is_stackable) {
        dirty_ |= PropertyWidgets::inputCount(edit_.count, 100);
    }
    
    // Charges for runes/wands
    if (item_type_->hasFlag(Domain::ItemFlag::ClientCharges)) {
        dirty_ |= PropertyWidgets::inputCharges(edit_.charges);
    }
    
    // Tier for OTBM v4+
    if (otbm_version_ >= 4) {
        dirty_ |= PropertyWidgets::inputTier(edit_.tier);
    }
}

void PropertyPanelRenderer::renderNormalSection() {
    renderCommonItemFields();
}

void PropertyPanelRenderer::renderContainerSection() {
    renderCommonItemFields();
    
    if (!item_) return;
    
    ImGui::Separator();
    
    // Get container info from ItemType.volume
    const auto& items = item_->getContainerItems();
    size_t const used = items.size();
    uint16_t volume = item_type_ ? item_type_->volume : 20;
    if (volume == 0) volume = 20;  // Fallback
    
    // Header with icon
    ImGui::Text(ICON_FA_BOX_OPEN " Container Contents: %zu / %u", used, volume);
    
    // Fixed grid: 5 columns, 4 visible rows (20 slots), scroll for more
    const int COLS = 5;
    const int VISIBLE_ROWS = 4;
    const float SLOT_SIZE = 36.0f;
    const float PADDING = 2.0f;
    
    // Fixed height for 4 rows, scroll if more
    float const child_height = VISIBLE_ROWS * (SLOT_SIZE + PADDING) + PADDING + 8.0f;
    
    ImGui::BeginChild("ContainerItems", ImVec2(0, child_height), true);
    
    // Render ALL slots (volume count), filled or empty
    for (size_t i = 0; i < volume; ++i) {
        if (i % COLS != 0) ImGui::SameLine(0, PADDING);
        
        Domain::Item const* slot_item = (i < items.size()) ? items[i].get() : nullptr;
        
        ImGui::PushID(static_cast<int>(i));
        
        ImVec2 const pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        
        // Slot background (dark)
        dl->AddRectFilled(pos, ImVec2(pos.x + SLOT_SIZE, pos.y + SLOT_SIZE), 
                          IM_COL32(40, 40, 40, 255));
        dl->AddRect(pos, ImVec2(pos.x + SLOT_SIZE, pos.y + SLOT_SIZE), 
                    IM_COL32(80, 80, 80, 255));
        
        // Draw item sprite if slot has item
        if (slot_item && sprite_manager_) {
            const Domain::ItemType* slot_type = slot_item->getType();
            if (slot_type) {
                if (auto* tex = sprite_manager_->getItemCompositor().getCompositedItemTexture(slot_type)) {
                    ImGui::Image((void*)(intptr_t)tex->id(), ImVec2(SLOT_SIZE, SLOT_SIZE));
                    
                    // Tooltip on hover
                    if (ImGui::IsItemHovered()) {
                        std::string const name = slot_type->name.empty() 
                            ? "Item " + std::to_string(slot_item->getServerId())
                            : slot_type->name + " (" + std::to_string(slot_item->getServerId()) + ")";
                        ImGui::SetTooltip("%s", name.c_str());
                    }
                } else {
                    // Fallback: colored square for item without texture
                    const float margin = 4.0f;
                    dl->AddRectFilled(
                        ImVec2(pos.x + margin, pos.y + margin),
                        ImVec2(pos.x + SLOT_SIZE - margin, pos.y + SLOT_SIZE - margin),
                        IM_COL32(100, 150, 200, 255)
                    );
                    ImGui::Dummy(ImVec2(SLOT_SIZE, SLOT_SIZE));
                }
            } else {
                ImGui::Dummy(ImVec2(SLOT_SIZE, SLOT_SIZE));
            }
        } else {
            // Empty slot - just advance cursor
            ImGui::Dummy(ImVec2(SLOT_SIZE, SLOT_SIZE));
        }
        
        ImGui::PopID();
    }
    
    ImGui::EndChild();
}

void PropertyPanelRenderer::renderWriteableSection() {
    renderCommonItemFields();
    
    ImGui::Separator();
    ImGui::Text("Text:");
    dirty_ |= PropertyWidgets::inputText(edit_.text, sizeof(edit_.text));
}

void PropertyPanelRenderer::renderSplashSection() {
    if (item_) {
        ImGui::Text("ID: %u", item_->getServerId());
    }
    ImGui::Separator();
    
    dirty_ |= PropertyWidgets::inputFluidType(edit_.fluid_type);
    
    ImGui::Separator();
    dirty_ |= PropertyWidgets::inputActionId(edit_.action_id);
    dirty_ |= PropertyWidgets::inputUniqueId(edit_.unique_id);
}

void PropertyPanelRenderer::renderDepotSection() {
    renderCommonItemFields();
    
    ImGui::Separator();
    
    // If we have a map with towns, show dropdown; otherwise fallback to number input
    if (map_) {
        const auto& towns = map_->getTowns();
        
        // Find current town name for preview
        std::string preview = "No Town";
        for (const auto& town : towns) {
            if (static_cast<int>(town.id) == edit_.depot_id) {
                preview = town.name;
                break;
            }
        }
        
        ImGui::Text("Depot Town:");
        if (ImGui::BeginCombo("##DepotTown", preview.c_str())) {
            // "No Town" option
            bool const is_none = (edit_.depot_id == 0);
            if (ImGui::Selectable("No Town", is_none)) {
                edit_.depot_id = 0;
                dirty_ = true;
            }
            if (is_none) {
                ImGui::SetItemDefaultFocus();
            }
            
            // Town options
            for (const auto& town : towns) {
                bool const is_selected = (static_cast<int>(town.id) == edit_.depot_id);
                if (ImGui::Selectable(town.name.c_str(), is_selected)) {
                    edit_.depot_id = static_cast<int>(town.id);
                    dirty_ = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        // Show depot ID for reference
        ImGui::TextDisabled("(Depot ID: %d)", edit_.depot_id);
    } else {
        // Fallback to raw ID input if no map available
        dirty_ |= PropertyWidgets::inputDepotId(edit_.depot_id);
    }
}

void PropertyPanelRenderer::renderDoorSection() {
    renderCommonItemFields();
    
    ImGui::Separator();
    dirty_ |= PropertyWidgets::inputDoorId(edit_.door_id);
}

void PropertyPanelRenderer::renderTeleportSection() {
    renderCommonItemFields();
    
    ImGui::Separator();
    dirty_ |= PropertyWidgets::inputPosition(
        edit_.tele_x, edit_.tele_y, edit_.tele_z,
        map_width_, map_height_
    );
}

void PropertyPanelRenderer::renderPodiumSection() {
    renderCommonItemFields();
    
    ImGui::Separator();
    dirty_ |= PropertyWidgets::inputDirection(edit_.direction);
    
    dirty_ |= ImGui::Checkbox("Show Outfit", &edit_.show_outfit);
    ImGui::SameLine();
    dirty_ |= ImGui::Checkbox("Show Mount", &edit_.show_mount);
    dirty_ |= ImGui::Checkbox("Show Platform", &edit_.show_platform);
    
    if (edit_.show_outfit) {
        ImGui::Separator();
        dirty_ |= PropertyWidgets::inputOutfit(edit_.outfit);
    }
}

void PropertyPanelRenderer::renderSpawnSection() {
    ImGui::Text("Spawn Point");
    ImGui::Separator();
    
    dirty_ |= PropertyWidgets::inputSpawnRadius(edit_.spawn_radius, 30);
    
    // Show spawn position if available
    if (spawn_) {
        ImGui::TextDisabled("Center: %d, %d, %d", 
            spawn_->position.x, spawn_->position.y, spawn_->position.z);
    }
}

void PropertyPanelRenderer::renderCreatureSection() {
    if (creature_) {
        ImGui::Text("Creature: %s", creature_->name.c_str());
    }
    ImGui::Separator();
    
    dirty_ |= PropertyWidgets::inputSpawnTime(edit_.spawn_time);
    dirty_ |= PropertyWidgets::inputDirection(edit_.direction);
}

} // namespace MapEditor::UI::Properties
