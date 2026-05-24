#pragma once
#include "Domain/SelectionSettings.h"
namespace MapEditor {
namespace AppLogic {
    class EditorSession;
}
}

namespace MapEditor::UI {

/**
 * Selection menu in main menu bar.
 * Provides Select All, Deselect, selection mode, and floor scope options.
 */
class SelectionMenu {
public:
    explicit SelectionMenu(Domain::SelectionSettings& settings);
    
    /**
     * Render the menu. Call inside ImGui::BeginMainMenuBar().
     * @param session Current editor session (for Select All/Deselect)
     */
    void render(AppLogic::EditorSession* session);

private:
    static void renderSelectionActions(AppLogic::EditorSession* session);
    void renderSelectionModeOptions();
    void renderFloorScopeOptions();
    
    Domain::SelectionSettings& settings_;
};

} // namespace MapEditor::UI
