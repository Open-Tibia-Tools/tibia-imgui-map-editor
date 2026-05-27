#include "UI/Panels/NewMapPanel.h"
#include "Core/Config.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <format>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace MapEditor {
namespace UI {

void NewMapPanel::initialize(Services::ClientVersionRegistry *registry) {
  registry_ = registry;
}

void NewMapPanel::reset() { name_touched_ = false; size_touched_ = false; }

bool NewMapPanel::render(State &state) {
  bool changed = false;
  auto avail = ImGui::GetContentRegionAvail();

  // Column widths: 3/5 left, 2/5 right
  float right_w = avail.x * RIGHT_RATIO - ImGui::GetStyle().ItemSpacing.x * 0.5f;
  float left_w  = avail.x - right_w - ImGui::GetStyle().ItemSpacing.x;

  // === GUIDED GLOW STATE ===
  bool name_glow    = !name_touched_;
  bool ver_glow     = name_touched_ && state.selected_template_index < 0;
  bool size_glow    = name_touched_ && state.selected_template_index >= 0 && !size_touched_;
  float pulse = (sinf(ImGui::GetTime() * 3.0f) + 1.0f) * 0.5f;

  auto applyGlow = [&](bool cond) {
    if (cond) {
      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.5f, 0.0f, 0.3f + pulse * 0.5f));
      ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.9f, 0.8f, 0.2f, 0.5f + pulse * 0.5f));
    }
  };
  auto popGlow = [](bool cond) { if (cond) ImGui::PopStyleColor(2); };

  constexpr ImVec4 label(0.55f, 0.58f, 0.62f, 1.0f);

  // =========================================================
  //  LEFT COLUMN
  // =========================================================
  float content_h = 200.0f;
  ImGui::BeginChild("##left", ImVec2(left_w, content_h), ImGuiChildFlags_None);

  // ---- Map Name ----
  ImGui::TextColored(label, ICON_FA_FILE " Map Name");
  name_buffer_ = state.map_name;
  applyGlow(name_glow);
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##mapname", &name_buffer_)) {
    state.map_name = name_buffer_;
    name_touched_ = changed = true;
  }
  popGlow(name_glow);
  ImGui::Spacing();

  // ---- Client Version ----
  ImGui::TextColored(label, ICON_FA_CODE_BRANCH " Client Version");
  applyGlow(ver_glow);
  ImGui::BeginDisabled(!name_touched_);
  renderClientVersionCombo(state);
  ImGui::EndDisabled();
  popGlow(ver_glow);
  ImGui::Spacing();

  // ---- Version Details (always visible) ----
  ImGui::Spacing();
  ImGui::BeginDisabled(state.selected_template_index < 0);

  float fw = 60.0f;
  // Labels
  ImGui::TextColored(label, "OTBM");      ImGui::SameLine(72);
  ImGui::TextColored(label, "Major");      ImGui::SameLine(144);
  ImGui::TextColored(label, "Minor");
  // Inputs
  ImGui::SetNextItemWidth(fw);
  int ov = (int)state.otbm_version;
  if (ImGui::InputInt("##otbm", &ov, 0, 0)) { state.otbm_version = (uint32_t)std::clamp(ov,1,4); changed=true; }
  ImGui::SameLine(0, 7);
  ImGui::SetNextItemWidth(fw);
  int ma = (int)state.items_major;
  if (ImGui::InputInt("##major", &ma, 0, 0)) { state.items_major = (uint32_t)std::max(0,ma); changed=true; }
  ImGui::SameLine(0, 7);
  ImGui::SetNextItemWidth(fw);
  int mi = (int)state.items_minor;
  if (ImGui::InputInt("##minor", &mi, 0, 0)) { state.items_minor = (uint32_t)std::max(0,mi); changed=true; }

  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // ---- Size (labels above, inputs in one row) ----
  applyGlow(size_glow);
  ImGui::BeginDisabled(state.selected_template_index < 0);
  // Labels
  ImGui::TextColored(label, "Size");
  ImGui::SameLine(90);
  ImGui::TextColored(label, "X");
  ImGui::SameLine(175);
  ImGui::TextColored(label, "Y");

  // Inputs
  static const char* sz[] = {"256","512","1024","2048","4096","8192","16384","32768","65000","Custom"};
  constexpr int preset_count = Config::UI::SIZE_PRESET_COUNT + 1; // +1 for Custom
  const char* preview = (state.size_preset_index >= 0 && state.size_preset_index < preset_count)
                            ? sz[state.size_preset_index] : "Custom";
  ImGui::SetNextItemWidth(80);
  if (ImGui::BeginCombo("##szpreset", preview)) {
    for (int i = 0; i < preset_count; ++i) {
      if (ImGui::Selectable(sz[i], state.size_preset_index == i)) {
        state.size_preset_index = i;
        size_touched_ = true;
        if (i < Config::UI::SIZE_PRESET_COUNT) {
          state.map_width  = Config::UI::SIZE_PRESETS[i];
          state.map_height = Config::UI::SIZE_PRESETS[i];
          changed = true;
        }
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine(0, 8);
  ImGui::SetNextItemWidth(80);
  int w = state.map_width;
  if (ImGui::InputInt("##w", &w, 0, 0)) {
    state.map_width = (uint16_t)std::clamp(w, Config::Map::MIN_SIZE, Config::Map::MAX_SIZE);
    state.size_preset_index = -1;
    size_touched_ = changed = true;
  }

  ImGui::SameLine(0, 8);
  ImGui::SetNextItemWidth(80);
  int h = state.map_height;
  if (ImGui::InputInt("##h", &h, 0, 0)) {
    state.map_height = (uint16_t)std::clamp(h, Config::Map::MIN_SIZE, Config::Map::MAX_SIZE);
    state.size_preset_index = -1;
    size_touched_ = changed = true;
  }
  ImGui::EndDisabled();
  popGlow(size_glow);

  ImGui::EndChild();

  ImGui::SameLine();

  // =========================================================
  //  RIGHT COLUMN — Description (compact, ~3 lines)
  // =========================================================
  ImGui::BeginChild("##right", ImVec2(right_w, content_h), ImGuiChildFlags_None);
  ImGui::TextColored(label, ICON_FA_FILE_LINES " Description");
  ImGui::Spacing();
  ImGui::InputTextMultiline("##desc", &state.description,
                            ImVec2(-1, ImGui::GetContentRegionAvail().y - 1),
                            ImGuiInputTextFlags_None);
  ImGui::EndChild();

  return changed;
}

void NewMapPanel::renderClientVersionCombo(State &state) {
  if (!registry_) return;

  const auto &tpls = registry_->getTemplates();
  std::string preview_str = "Select version...";
  if (state.selected_template_index >= 0 && state.selected_template_index < (int)tpls.size())
    preview_str = std::format("{} ({})", tpls[state.selected_template_index].name, tpls[state.selected_template_index].version);

  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##ver", preview_str.c_str())) {
    for (size_t i = 0; i < tpls.size(); ++i) {
      ImGui::PushID((int)i);
      std::string label = std::format("{} ({})", tpls[i].name, tpls[i].version);
      if (ImGui::Selectable(label.c_str(), state.selected_template_index == (int)i)) {
        state.selected_template_index = (int)i;
        state.otbm_version  = tpls[i].otbm_versions.empty()
            ? 2 : *std::max_element(tpls[i].otbm_versions.begin(), tpls[i].otbm_versions.end());
        state.items_major   = tpls[i].otb_major;
        state.items_minor   = tpls[i].otb_id;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
}

} // namespace UI
} // namespace MapEditor
