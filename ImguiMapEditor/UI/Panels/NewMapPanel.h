#pragma once
#include "Core/Config.h"
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
    uint16_t map_width = Config::Map::DEFAULT_MAP_SIZE;
    uint16_t map_height = Config::Map::DEFAULT_MAP_SIZE;
    int selected_template_index = -1;
    uint32_t otbm_version = 2;
    uint32_t items_major = 1;
    uint32_t items_minor = 1;
    std::string description = "Made with Tibia Imgui Map Editor!";
    int size_preset_index = 6;
  };

  void initialize(Services::ClientVersionRegistry *registry);

  void reset();

  bool render(State &state);

private:
  bool renderClientVersionCombo(State &state);

  Services::ClientVersionRegistry *registry_ = nullptr;
  std::string name_buffer_{"Untitled"};
  bool version_details_expanded_ = false;
  bool size_touched_ = false;
};

} // namespace UI
} // namespace MapEditor
