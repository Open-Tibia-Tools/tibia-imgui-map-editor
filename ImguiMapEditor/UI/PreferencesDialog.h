#pragma once
#include "Services/SecondaryClientConstants.h"
#include <filesystem>
#include <functional>
#include <string>

namespace MapEditor {

namespace Services {
class SecondaryClientData;
class HotkeyRegistry;
} // namespace Services

namespace UI {

/**
 * Preferences dialog with settings tabs.
 */
class PreferencesDialog {
public:
  enum class Result { None, Closed };

  void show();
  Result render();
  bool isOpen() const { return is_open_; }

  // === Secondary Client Callbacks ===

  /**
   * Set callback for loading secondary client.
   * @param callback Receives folder_path containing Tibia.dat, Tibia.spr,
   * items.otb
   */
  using LoadSecondaryCallback =
      std::function<bool(const std::filesystem::path &folder_path)>;
  void setLoadSecondaryCallback(LoadSecondaryCallback callback) {
    on_load_secondary_ = std::move(callback);
  }

  /**
   * Set callback for unloading secondary client.
   */
  using UnloadSecondaryCallback = std::function<void()>;
  void setUnloadSecondaryCallback(UnloadSecondaryCallback callback) {
    on_unload_secondary_ = std::move(callback);
  }

  /**
   * Set callback for toggling secondary client active state.
   */
  using ToggleSecondaryCallback = std::function<void(bool active)>;
  void setToggleSecondaryCallback(ToggleSecondaryCallback callback) {
    on_toggle_secondary_ = std::move(callback);
  }

  /**
   * Set secondary client data provider (for status display).
   */
  void setSecondaryClientProvider(Services::SecondaryClientProvider provider) {
    secondary_client_.setProvider(std::move(provider));
  }

  /**
   * Set hotkey registry for editing.
   */
  void setHotkeyRegistry(Services::HotkeyRegistry *registry) {
    hotkey_registry_ = registry;
  }

private:
  bool is_open_ = false;
  bool should_open_ = false;

  // Secondary client state
  Services::SecondaryClientHandle secondary_client_;
  char secondary_folder_path_[512] = "";
  std::string secondary_error_;

  // Hotkey editing state
  Services::HotkeyRegistry *hotkey_registry_ = nullptr;
  bool hotkeys_dirty_ = false;

  // Callbacks
  LoadSecondaryCallback on_load_secondary_;
  UnloadSecondaryCallback on_unload_secondary_;
  ToggleSecondaryCallback on_toggle_secondary_;

  // Tab renderers
  static void renderEditorTab();
  void renderSecondaryClientTab();
  void renderHotkeysTab();
};

} // namespace UI
} // namespace MapEditor
