#pragma once
#include "Services/ClientVersionRegistry.h"
#include <cstdint>
#include <string>

namespace MapEditor {
namespace UI {

class NewMapPanel {
public:
  static constexpr float LEFT_RATIO = 0.60f;
  static constexpr float RIGHT_RATIO = 0.40f;

  struct State {
    std::string map_name = "Untitled";
    uint16_t map_width = 16384;
    uint16_t map_height = 16384;
    int selected_template_index = -1;
    uint32_t otbm_version = 2;
    uint32_t items_major = 0;
    uint32_t items_minor = 0;
    std::string description = "Made with Tibia Imgui Map Editor!";
    int size_preset_index = 6;
  };

  void initialize(Services::ClientVersionRegistry *registry);

  void reset();

  bool render(State &state);

private:
  void renderClientVersionCombo(State &state);

  Services::ClientVersionRegistry *registry_ = nullptr;
  std::string name_buffer_{"Untitled"};
  bool version_details_expanded_ = false;
  bool name_touched_ = false;
  bool size_touched_ = false;
};

} // namespace UI
} // namespace MapEditor
