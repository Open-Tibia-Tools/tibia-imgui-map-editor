#pragma once
#include "UI/Panels/NewMapPanel.h"
#include <functional>

namespace MapEditor {
namespace UI {

/**
 * Standalone modal dialog for creating new maps.
 * Uses NewMapPanel as the content component.
 *
 * Two-column layout:
 *   Left: Map Name, Client Version (template loader), Version Details, Map Size
 *   Right: Description (multi-line)
 *   Footer: Cancel, Create Map
 *
 * Tabs: OTBM (default), SEC (placeholder "Coming soon")
 */
class NewMapDialog {
public:
  using OnConfirmCallback = std::function<void(const NewMapPanel::State &)>;

  NewMapDialog() = default;

  void initialize(Services::ClientVersionRegistry *registry);

  void show();
  void render();

  bool isVisible() const { return visible_; }

  void setOnConfirm(OnConfirmCallback callback) {
    on_confirm_ = std::move(callback);
  }

private:
  bool visible_ = false;
  NewMapPanel panel_;
  NewMapPanel::State state_;
  OnConfirmCallback on_confirm_;
};

} // namespace UI
} // namespace MapEditor
