#include "SessionLifecycleManager.h"
#include <algorithm>
#include <functional>
#include <iterator>
#include <spdlog/spdlog.h>
#include "Application/AppStateManager.h"
#include "Controllers/WorkspaceController.h"
#include "Rendering/Frame/RenderingManager.h"

namespace MapEditor::AppLogic {

void SessionLifecycleManager::requestCloseTab(int index) {
  spdlog::info(
      "SessionLifecycle: Deferring close of tab {} to processDeferredActions",
      index);
  pending_close_tab_indices_.push_back(index);
}

void SessionLifecycleManager::queueSessionsForDestruction(
    std::vector<std::unique_ptr<EditorSession>> sessions) {
  pending_sessions_to_destroy_.insert(pending_sessions_to_destroy_.end(),
                                      std::make_move_iterator(sessions.begin()),
                                      std::make_move_iterator(sessions.end()));
}

bool SessionLifecycleManager::extractDeferredSessions(
    MapTabManager &tab_manager) {
  bool extracted_any = false;

  // Handle pending tab close requests (deferred)
  if (!pending_close_tab_indices_.empty()) {
    // Sort in descending order so removing higher indices doesn't affect lower
    // ones
    std::sort(pending_close_tab_indices_.begin(),
              pending_close_tab_indices_.end(), std::greater<int>());

    // Remove duplicates
    pending_close_tab_indices_.erase(
        std::unique(pending_close_tab_indices_.begin(),
                    pending_close_tab_indices_.end()),
        pending_close_tab_indices_.end());

    for (int index : pending_close_tab_indices_) {
      // Validate index as it might have changed if multiple events fired
      if (index >= 0 && index < tab_manager.getTabCount()) {
        spdlog::info(
            "SessionLifecycle: Extracting session {} for deferred destruction",
            index);

        auto session_ptr = tab_manager.extractSession(index);
        if (session_ptr) {
          pending_sessions_to_destroy_.push_back(std::move(session_ptr));
          extracted_any = true;
        }
      }
    }

    pending_close_tab_indices_.clear();
  }

  return extracted_any;
}

void SessionLifecycleManager::destroyPendingSessions(
    Rendering::RenderingManager &rendering_manager) {
  if (!pending_sessions_to_destroy_.empty()) {
    spdlog::info("SessionLifecycle: Destroying {} deferred sessions",
                 pending_sessions_to_destroy_.size());

    // Clean up render states for all sessions about to be destroyed
    for (const auto &session : pending_sessions_to_destroy_) {
      if (session) {
        rendering_manager.destroyRenderState(session->getID());
      }
    }

    // Clear the vector, destroying the sessions
    // This happens AFTER render() so OpenGL resources are destroyed safely
    pending_sessions_to_destroy_.clear();

    spdlog::info("SessionLifecycle: Sessions destroyed successfully");
  }
}

bool SessionLifecycleManager::hasPendingDestruction() const {
  return !pending_sessions_to_destroy_.empty();
}

void SessionLifecycleManager::processDeferredActions(
    MapTabManager &tab_manager, Rendering::RenderingManager &rendering_manager,
    Presentation::WorkspaceController *workspace,
    AppStateManager &state_manager, std::function<void()> cleanup_callback) {

  // 1. Extract sessions that need closing (updates TabManager state)
  bool extracted_any = extractDeferredSessions(tab_manager);

  // 2. Clear UI references if needed (BEFORE destroying sessions)
  if (extracted_any || hasPendingDestruction()) {
    if (tab_manager.getActiveSession() == nullptr) {
      if (workspace) {
        workspace->unbindSession();
      }
    }
  }

  // 3. Destroy the sessions (safe to do now that UI is detached)
  destroyPendingSessions(rendering_manager);

  // 4. Return to welcome screen if no tabs left
  if (tab_manager.getTabCount() == 0 &&
      state_manager.isInState(AppStateManager::State::Editor)) {
    // Clean up client resources before transitioning
    if (cleanup_callback) {
      spdlog::info("SessionLifecycle: Cleaning up client resources");
      cleanup_callback();
    }
    state_manager.transition(AppStateManager::State::Startup);
  }
}

} // namespace MapEditor::AppLogic
