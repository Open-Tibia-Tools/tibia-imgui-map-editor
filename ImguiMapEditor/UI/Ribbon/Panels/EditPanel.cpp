#include "EditPanel.h"
#include "Application/MapTabManager.h"
#include "Domain/Position.h"
#include "IconsFontAwesome6.h"
#include "UI/Ribbon/Utils/RibbonUtils.h"
#include <imgui.h>
#include <string>
#include <format>

namespace MapEditor {
namespace UI {
namespace Ribbon {

namespace {
std::string MakeSelectionTooltip(const char* action, const char* shortcut, size_t count) {
    if (count > 0) {
        return std::format("{} {} items ({})", action, count, shortcut);
    }
    return std::format("{} ({})", action, shortcut);
}
} // namespace

EditPanel::EditPanel(AppLogic::MapTabManager *tab_manager)
    : tab_manager_(tab_manager) {}

void EditPanel::Render() {
  bool const has_session = tab_manager_ && tab_manager_->getActiveSession();
  bool const has_selection =
      has_session && !tab_manager_->getActiveSession()->getSelectionService().isEmpty();
  bool const can_paste = tab_manager_ && tab_manager_->getClipboard().canPaste();
  bool const can_undo = has_session && tab_manager_->getActiveSession()->canUndo();
  bool const can_redo = has_session && tab_manager_->getActiveSession()->canRedo();

  // Undo/Redo
  std::string undo_tooltip = "Undo (Ctrl+Z)";
  if (can_undo) {
      // Future: Get actual action name from history manager
      // For now, at least show it's available
      undo_tooltip = "Undo last action (Ctrl+Z)";
  }

  Utils::RenderButton(ICON_FA_ROTATE_LEFT, nullptr, can_undo, undo_tooltip.c_str(),
                      "Nothing to undo", [this]() {
                        if (tab_manager_)
                          tab_manager_->getActiveSession()->undo();
                      });

  ImGui::SameLine();

  std::string redo_tooltip = "Redo (Ctrl+Y)";
  if (can_redo) {
      redo_tooltip = "Redo last undone action (Ctrl+Y)";
  }

  Utils::RenderButton(ICON_FA_ROTATE_RIGHT, nullptr, can_redo, redo_tooltip.c_str(),
                      "Nothing to redo", [this]() {
                        if (tab_manager_)
                          tab_manager_->getActiveSession()->redo();
                      });

  ImGui::SameLine();
  Utils::RenderSeparator();
  ImGui::SameLine();

  size_t selection_count = 0;
  if (has_session) {
      selection_count = tab_manager_->getActiveSession()->getSelectionService().size();
  }

  // Cut/Copy/Paste
  std::string const cut_tooltip = MakeSelectionTooltip("Cut", "Ctrl+X", selection_count);

  Utils::RenderButton(ICON_FA_SCISSORS, nullptr, has_selection, cut_tooltip.c_str(),
                      "Select items first", [this]() {
                        if (tab_manager_)
                          tab_manager_->getClipboard().cut(
                              *tab_manager_->getActiveSession());
                      });

  ImGui::SameLine();

  std::string const copy_tooltip = MakeSelectionTooltip("Copy", "Ctrl+C", selection_count);

  Utils::RenderButton(ICON_FA_COPY, nullptr, has_selection, copy_tooltip.c_str(),
                      "Select items first", [this]() {
                        if (tab_manager_)
                          tab_manager_->getClipboard().copy(
                              *tab_manager_->getActiveSession());
                      });

  ImGui::SameLine();

  // Determine paste tooltip text
  std::string paste_tooltip = "Paste (Ctrl+V)";
  if (can_paste) {
      size_t clipboard_size = tab_manager_->getClipboard().getItemCount();
      if (clipboard_size > 0) {
          paste_tooltip = std::format("Paste {} items from clipboard (Ctrl+V)", clipboard_size);
      } else {
          paste_tooltip = "Paste items from clipboard (Ctrl+V)";
      }
  }

  Utils::RenderButton(ICON_FA_PASTE, nullptr, can_paste, paste_tooltip.c_str(),
                      "Clipboard is empty", [this]() {
                        if (tab_manager_) {
                          auto *session = tab_manager_->getActiveSession();
                          if (session) {
                            auto &view = session->getViewState();
                            Domain::Position const target_pos{
                                static_cast<int>(view.camera_x / 32.0f),
                                static_cast<int>(view.camera_y / 32.0f),
                                static_cast<int16_t>(view.current_floor)};
                            tab_manager_->getClipboard().paste(*session,
                                                               target_pos);
                          }
                        }
                      });

  ImGui::SameLine();
  Utils::RenderSeparator();
  ImGui::SameLine();

  // Delete
  std::string const delete_tooltip = MakeSelectionTooltip("Delete", "Del", selection_count);

  Utils::RenderButton(ICON_FA_TRASH, nullptr, has_selection, delete_tooltip.c_str(),
                      "Select items first", [this]() {
                        if (tab_manager_)
                          tab_manager_->getActiveSession()->deleteSelection();
                      });

  ImGui::SameLine();

  // Deselect
  Utils::RenderButton(ICON_FA_ERASER, nullptr, has_selection, "Deselect All (Esc)",
                      "Nothing selected", [this]() {
                        if (tab_manager_)
                          tab_manager_->getActiveSession()->clearSelection();
                      });
}

} // namespace Ribbon
} // namespace UI
} // namespace MapEditor
