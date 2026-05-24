#include "RibbonController.h"
#include <imgui_internal.h>

namespace MapEditor {
namespace UI {
namespace Ribbon {

RibbonController::RibbonController() = default;

void RibbonController::AddPanel(std::unique_ptr<IRibbonPanel> panel) {
    if (panel) {
        panels_.push_back(std::move(panel));
    }
}

void RibbonController::Render() {
    if (panels_.empty()) {
        return;
    }
    
    // On first frame, set up initial dock layout in the main dockspace
    if (first_frame_) {
        SetupInitialDockLayout();
        first_frame_ = false;
    }
    
    // Render each panel as a regular dockable window
    // These participate in the main application dockspace (DockSpaceOverViewport)
    for (auto& panel : panels_) {
        ImGuiWindowFlags const panel_flags = 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
        
        ImGui::SetNextWindowSize(ImVec2(250, ribbon_height_), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin(panel->GetPanelID(), nullptr, panel_flags)) {
            panel->Render();
        }
        ImGui::End();
    }
}

void RibbonController::SetupInitialDockLayout() {
    // Check if imgui.ini already has saved docking info for our panels
    // by checking if window settings exist for first panel
    if (!panels_.empty()) {
        ImGuiID const window_id = ImHashStr(panels_[0]->GetPanelID());
        ImGuiWindowSettings const* settings = ImGui::FindWindowSettingsByID(window_id);
        if (settings && settings->DockId != 0) {
            // Panel has saved dock settings from imgui.ini - don't override
            return;
        }
    }
    
    // Find the main dockspace
    ImGuiContext& g = *GImGui;
    ImGuiID main_dockspace_id = 0;
    for (int n = 0; n < g.DockContext.Nodes.Data.Size; n++) {
        ImGuiDockNode const* node = static_cast<ImGuiDockNode*>(g.DockContext.Nodes.Data[n].val_p);
        if (node && node->IsDockSpace() && node->HostWindow) {
            main_dockspace_id = node->ID;
            break;
        }
    }
    
    if (main_dockspace_id == 0) {
        return;
    }
    
    // Split the main dockspace: create a top node for ribbon panels
    ImGuiID top_id, remaining_id;
    ImGui::DockBuilderSplitNode(main_dockspace_id, ImGuiDir_Up, 0.08f, &top_id, &remaining_id);
    
    // Dock all ribbon panels into the top node
    for (auto& panel : panels_) {
        ImGui::DockBuilderDockWindow(panel->GetPanelID(), top_id);
    }
    
    ImGui::DockBuilderFinish(main_dockspace_id);
}

} // namespace Ribbon
} // namespace UI
} // namespace MapEditor
