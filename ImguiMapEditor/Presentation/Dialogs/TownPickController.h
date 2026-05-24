#pragma once
#include "UI/Dialogs/EditTownsDialog.h"
#include "UI/Map/MapPanel.h"
#include "Domain/Position.h"
#include "ImGuiNotify.hpp"
#include <imgui.h>
#include <glm/glm.hpp>

namespace MapEditor::Presentation {

/**
 * Handles town dialog pick mode for selecting temple positions.
 * Extracts pick mode logic from Application::render().
 */
class TownPickController {
public:
    struct Context {
        UI::EditTownsDialog* dialog = nullptr;
        UI::MapPanel* map_panel = nullptr;
    };
    
    /**
     * Process pick mode - detect map clicks and set temple position.
     */
    static void processPickMode(const Context& ctx);
};

} // namespace MapEditor::Presentation
