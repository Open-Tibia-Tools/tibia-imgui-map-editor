#pragma once
#include "Services/ClipboardService.h"
#include "Domain/CopyBuffer.h"
#include "EditorSession.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace MapEditor::Services {
class ClientDataService;
}

namespace MapEditor::Rendering {
class RenderingManager;
}

namespace MapEditor::AppLogic {

/**
 * Manages multiple editor sessions (tabs).
 * Owns the shared CopyBuffer for cross-tab clipboard operations.
 */
class MapTabManager {
public:
  MapTabManager();
  ~MapTabManager();

  // Set client data service for session creation
  void setClientData(Services::ClientDataService *client_data) {
    client_data_ = client_data;
  }

  // Set rendering manager for render state lifecycle
  void setRenderingManager(Rendering::RenderingManager *rendering_manager) {
    rendering_manager_ = rendering_manager;
  }

  // Non-copyable
  MapTabManager(const MapTabManager &) = delete;
  MapTabManager &operator=(const MapTabManager &) = delete;

  // Tab management
  /**
   * Open an existing map as a new tab.
   * @param map The loaded map to open
   * @param path File path (empty for new/untitled maps)
   * @return Tab index of the opened map
   */
  int openMap(std::unique_ptr<Domain::ChunkedMap> map,
              const std::filesystem::path &path = {});

  /**
   * Create a new empty map as a new tab.
   * @return Tab index of the new map
   */
  int createNewMap(uint16_t width, uint16_t height, uint32_t version);

  /**
   * Close a tab by index.
   * @param index Tab index to close
   */
  void closeTab(int index);

  /**
   * Extract all sessions from the manager without destroying them.
   * Used for deferred destruction to avoid OpenGL crashes.
   */
  std::vector<std::unique_ptr<EditorSession>> extractAllSessions();

  /**
   * Extract a session from the manager without destroying it.
   * Used for deferred destruction to avoid OpenGL crashes.
   * @param index Tab index to extract
   * @return Unique pointer to the extracted session, or nullptr if invalid
   * index
   */
  std::unique_ptr<EditorSession> extractSession(int index);

  // Active tab
  void setActiveTab(int index);
  int getActiveTabIndex() const { return active_index_; }
  EditorSession *getActiveSession();
  const EditorSession *getActiveSession() const;

  // Tab queries
  int getTabCount() const { return static_cast<int>(sessions_.size()); }
  EditorSession *getSession(int index);
  const EditorSession *getSession(int index) const;
  bool hasUnsavedChanges() const;

  // Clipboard (shared across all tabs)
  ClipboardService &getClipboard() { return clipboard_; }
  const ClipboardService &getClipboard() const { return clipboard_; }
  Domain::CopyBuffer &getCopyBuffer() { return copybuffer_; }

  // Unnamed map counter (monotonic, never reuses, resets on app restart)
  uint32_t nextUnnamedNumber() { return next_unnamed_number_++; }

  // Events - callback receives (old_index, new_index) for proper state
  // save/restore
  using TabChangedCallback = std::function<void(int old_index, int new_index)>;
  void setTabChangedCallback(TabChangedCallback cb) {
    on_tab_changed_ = std::move(cb);
  }

  // Session modification callback - fires when any session is modified (now
  // takes bool)
  using SessionModifiedCallback = std::function<void(bool modified)>;
  void setSessionModifiedCallback(SessionModifiedCallback cb) {
    on_session_modified_ = std::move(cb);
  }

private:
  std::vector<std::unique_ptr<EditorSession>> sessions_;
  int active_index_ = -1;

  Domain::CopyBuffer copybuffer_; // Shared across all tabs
  ClipboardService clipboard_;
  TabChangedCallback on_tab_changed_;
  SessionModifiedCallback on_session_modified_;
  Services::ClientDataService *client_data_ = nullptr;
  Rendering::RenderingManager *rendering_manager_ = nullptr;
  uint32_t next_unnamed_number_ = 1;
};

} // namespace MapEditor::AppLogic
