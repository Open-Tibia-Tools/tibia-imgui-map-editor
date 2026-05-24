#pragma once
#include "Domain/Item.h"
#include "BasePropertiesDialog.h"

namespace MapEditor::Services {
    class SpriteManager;
}

namespace MapEditor::UI {

/**
 * Modal dialog for editing item properties.
 * Edits: Action ID, Unique ID, Text, Teleport destination, Door ID.
 * Shows "Contents" tab for containers.
 */
class ItemPropertiesDialog : public BasePropertiesDialog {
public:
    ItemPropertiesDialog();
    
    /**
     * Set sprite manager for container rendering.
     */
    void setSpriteManager(Services::SpriteManager* sm) { sprite_manager_ = sm; }
    
    /**
     * Open dialog for the given item.
     */
    void open(Domain::Item* item, SaveCallback on_save = nullptr);
    
    // BasePropertiesDialog implementation
    void renderContent() override;
    void onSave() override;
    void onClose() override;

    // Expose render() from base class with specific title
    void render() { BasePropertiesDialog::render("Item Properties", ImVec2(Config::UI::ITEM_PROPS_WINDOW_W, Config::UI::ITEM_PROPS_WINDOW_H)); }
    
private:
    void renderContentsTab();
    static bool renderSlotButton(Domain::Item* item, float size, bool selected);
    
    Domain::Item* current_item_ = nullptr;
    Services::SpriteManager* sprite_manager_ = nullptr;
    
    // Editable values
    int action_id_ = 0;
    int unique_id_ = 0;
    char text_buffer_[256] = {0};
    int tele_x_ = 0;
    int tele_y_ = 0;
    int tele_z_ = 0;
    int door_id_ = 0;
    
    // Container tab state
    int selected_slot_ = -1;
};

} // namespace MapEditor::UI
