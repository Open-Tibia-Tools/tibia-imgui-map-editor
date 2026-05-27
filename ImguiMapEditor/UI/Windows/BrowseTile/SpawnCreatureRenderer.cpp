#include "SpawnCreatureRenderer.h"
#include "UI/Core/Theme.h"
#include "Domain/Creature.h"
#include "Domain/Spawn.h"
#include "Rendering/Core/Texture.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "UI/Utils/PreviewUtils.hpp"
namespace MapEditor::UI::BrowseTile {

namespace SC = SemanticColors;

SpawnCreatureRenderer::SpawnCreatureRenderer(
    Services::SpriteManager *spriteManager,
    Services::ClientDataService *clientData)
    : sprite_manager_(spriteManager), client_data_(clientData) {}

void SpawnCreatureRenderer::setContext(
    Services::SpriteManager *spriteManager,
    Services::ClientDataService *clientData) {
  sprite_manager_ = spriteManager;
  client_data_ = clientData;
}

void SpawnCreatureRenderer::render(const Domain::Tile *current_tile,
                                   bool &spawn_selected,
                                   bool &creature_selected,
                                   int &selected_index) {
  if (!current_tile)
    return;

  ImGui::Text("Spawn/Creature");

  // Fixed height - always visible, no scrolling needed
  float spawn_box_height = 105.0f;
  ImGui::BeginChild("SpawnCreatureList", ImVec2(0, spawn_box_height), true);

  const Domain::Spawn *spawn =
      current_tile->hasSpawn() ? current_tile->getSpawn() : nullptr;
  const Domain::Creature *creature =
      current_tile->hasCreature() ? current_tile->getCreature() : nullptr;

  const float preview_size = 64.0f;

  if (ImGui::BeginTable("SpawnCreatureTable", 2,
                        ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchSame)) {

    ImGui::TableNextRow();

    // ===== LEFT CELL: Spawn =====
    ImGui::TableNextColumn();
    {
      float cell_width = ImGui::GetContentRegionAvail().x;
      float offset_x = (cell_width - preview_size) / 2.0f;

      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
      ImVec2 box_min = ImGui::GetCursorScreenPos();
      ImVec2 box_max =
          ImVec2(box_min.x + preview_size, box_min.y + preview_size);

      if (spawn) {
        ImU32 fill_color = spawn_selected ? IM_COL32(255, 220, 80, 255)
                                          : IM_COL32(200, 180, 50, 255);
        ImGui::GetWindowDrawList()->AddRectFilled(box_min, box_max, fill_color);
        ImGui::GetWindowDrawList()->AddRect(box_min, box_max,
                                            IM_COL32(255, 220, 80, 255));

        ImVec2 s_size = ImGui::CalcTextSize("S");
        ImVec2 s_pos = ImVec2(box_min.x + (preview_size - s_size.x) / 2,
                              box_min.y + (preview_size - s_size.y) / 2);
        ImGui::GetWindowDrawList()->AddText(s_pos, IM_COL32(0, 0, 0, 255), "S");
      } else {
        ImGui::GetWindowDrawList()->AddRectFilled(box_min, box_max,
                                                  IM_COL32(50, 50, 50, 255));
        ImGui::GetWindowDrawList()->AddRect(box_min, box_max,
                                            IM_COL32(80, 80, 80, 255));
        ImVec2 dash_size = ImGui::CalcTextSize("-");
        ImVec2 dash_pos = ImVec2(box_min.x + (preview_size - dash_size.x) / 2,
                                 box_min.y + (preview_size - dash_size.y) / 2);
        ImGui::GetWindowDrawList()->AddText(dash_pos,
                                            IM_COL32(100, 100, 100, 255), "-");
      }

      ImGui::PushID("spawn_select");
      if (ImGui::InvisibleButton("spawn_btn",
                                 ImVec2(preview_size, preview_size)) &&
          spawn) {
        spawn_selected = true;
        creature_selected = false;
        selected_index = -1;
      }
      ImGui::PopID();

      std::string spawn_text =
          spawn ? ("r=" + std::to_string(spawn->radius)) : "-";
      float text_width = ImGui::CalcTextSize(spawn_text.c_str()).x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           (cell_width - text_width) / 2.0f);
      if (spawn) {
        ImGui::Text("%s", spawn_text.c_str());
      } else {
        ImGui::TextDisabled("%s", spawn_text.c_str());
      }
    }

    // ===== RIGHT CELL: Creature =====
    ImGui::TableNextColumn();
    {
      float cell_width = ImGui::GetContentRegionAvail().x;
      float offset_x = (cell_width - preview_size) / 2.0f;

      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
      ImVec2 box_min = ImGui::GetCursorScreenPos();
      ImVec2 box_max =
          ImVec2(box_min.x + preview_size, box_min.y + preview_size);

      bool rendered_sprite = false;
      if (creature && client_data_ && sprite_manager_) {
        auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, creature->name);
        if (preview && preview.texture) {
          ImGui::Image((void *)(intptr_t)preview.texture->id(),
                       ImVec2(preview_size, preview_size));
          if (creature_selected) {
            ImGui::GetWindowDrawList()->AddRect(
                box_min, box_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
          }
          rendered_sprite = true;
        }
      }

      if (!rendered_sprite) {
        if (creature) {
          ImGui::GetWindowDrawList()->AddRectFilled(box_min, box_max,
                                                    IM_COL32(80, 80, 80, 255));
          ImVec2 c_size = ImGui::CalcTextSize("C");
          ImVec2 c_pos = ImVec2(box_min.x + (preview_size - c_size.x) / 2,
                                box_min.y + (preview_size - c_size.y) / 2);
          ImGui::GetWindowDrawList()->AddText(
              c_pos, IM_COL32(200, 200, 200, 255), "C");
        } else {
          ImGui::GetWindowDrawList()->AddRectFilled(box_min, box_max,
                                                    IM_COL32(50, 50, 50, 255));
          ImVec2 dash_size = ImGui::CalcTextSize("-");
          ImVec2 dash_pos =
              ImVec2(box_min.x + (preview_size - dash_size.x) / 2,
                     box_min.y + (preview_size - dash_size.y) / 2);
          ImGui::GetWindowDrawList()->AddText(
              dash_pos, IM_COL32(100, 100, 100, 255), "-");
        }
        ImGui::GetWindowDrawList()->AddRect(box_min, box_max,
                                            IM_COL32(80, 80, 80, 255));
        ImGui::Dummy(ImVec2(preview_size, preview_size));
      }

      ImGui::SetCursorScreenPos(box_min);
      ImGui::PushID("creature_select");
      if (ImGui::InvisibleButton("creature_btn",
                                 ImVec2(preview_size, preview_size)) &&
          creature) {
        creature_selected = true;
        spawn_selected = false;
        selected_index = -1;
      }
      ImGui::PopID();

      std::string creature_text = creature ? creature->name : "-";
      float text_width = ImGui::CalcTextSize(creature_text.c_str()).x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           (cell_width - text_width) / 2.0f);
      if (creature) {
        ImGui::Text("%s", creature_text.c_str());
      } else {
        ImGui::TextDisabled("%s", creature_text.c_str());
      }
    }

    ImGui::EndTable();
  }
  ImGui::EndChild();
}

} // namespace MapEditor::UI::BrowseTile
