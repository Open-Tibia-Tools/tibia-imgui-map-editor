#pragma once
#include "UI/DTOs/SelectedMapInfo.h"

namespace MapEditor {
namespace UI {

/**
 * Renders the Selected Map information panel for the StartupDialog.
 */
class SelectedMapPanel {
public:
  void setMapInfo(const SelectedMapInfo &info) { map_info_ = info; }
  const SelectedMapInfo &getMapInfo() const { return map_info_; }

  void render() const;

private:
  SelectedMapInfo map_info_;
};

} // namespace UI
} // namespace MapEditor
