#pragma once
#include "Application/EditorSession.h"
#include "Application/MapTabManager.h"
#include <functional>
#include <memory>
#include <vector>

// Forward declarations
namespace MapEditor {
class AppStateManager;
}
namespace MapEditor::Presentation {
class WorkspaceController;
}
namespace MapEditor::Rendering {
class RenderingManager;
}

namespace MapEditor::AppLogic {

/**
 * Manages the lifecycle of EditorSessions, specifically deferred destruction.
 * This is required to ensure OpenGL resources are destroyed on the main thread
 * after the rendering loop, preventing crashes and context issues.
 */
class SessionLifecycleManager {
public:
  SessionLifecycleManager() = default;

  /**
   * Request closing a tab by index.
   * Defers the actual extraction and destruction to processDeferredActions.
   */
  void requestCloseTab(int index);

  /**
   * Directly queue sessions for deferred destruction.
   * Useful when performing bulk operations like "Close All".
   */
  void queueSessionsForDestruction(
      std::vector<std::unique_ptr<EditorSession>> sessions);

  /**
   * Extract sessions that are pending closure from the TabManager.
   * Does NOT destroy them yet. Call destroyPendingSessions() after clearing UI
   * references.
   *
   * @param tab_manager The tab manager to extract sessions from.
   * @return true if any new sessions were extracted.
   */
  bool extractDeferredSessions(MapTabManager &tab_manager);

  /**
   * Check if there are any sessions waiting to be destroyed.
   */
  bool hasPendingDestruction() const;

  /**
   * Destroy all pending sessions.
   * Should be called AFTER clearing any UI references to these sessions.
   *
   * @param rendering_manager Rendering manager to cleanup render states.
   */
  void destroyPendingSessions(Rendering::RenderingManager &rendering_manager);

  /**
   * Process all deferred actions in the correct order.
   * Encapsulates the full workflow: extract sessions, unbind UI, destroy, and
   * transition state if needed.
   *
   * @param cleanup_callback Optional callback invoked when last tab closes,
   *        used to release client resources before returning to welcome screen.
   */
  void processDeferredActions(MapTabManager &tab_manager,
                              Rendering::RenderingManager &rendering_manager,
                              Presentation::WorkspaceController *workspace,
                              AppStateManager &state_manager,
                              std::function<void()> cleanup_callback = nullptr);

private:
  std::vector<int> pending_close_tab_indices_;
  std::vector<std::unique_ptr<EditorSession>> pending_sessions_to_destroy_;
};

} // namespace MapEditor::AppLogic
