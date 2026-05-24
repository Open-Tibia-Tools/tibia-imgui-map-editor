#include "ItemsListRenderer.h"
#include "Application/EditorSession.h"
#include "Domain/ChunkedMap.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Tile.h"
#include "Rendering/Core/Texture.h"
#include "Services/SpriteManager.h"
#include "../../ext/fontawesome6/IconsFontAwesome6.h"
#include <format>

namespace MapEditor::UI::BrowseTile {

ItemsListRenderer::ItemsListRenderer(Domain::ChunkedMap *map,
                                     Services::SpriteManager *spriteManager,
                                     AppLogic::EditorSession *session)
    : map_(map), sprite_manager_(spriteManager), session_(session) {}

void ItemsListRenderer::setContext(Domain::ChunkedMap *map,
                                   Services::SpriteManager *spriteManager,
                                   AppLogic::EditorSession *session) {
  map_ = map;
  sprite_manager_ = spriteManager;
  session_ = session;
}

void ItemsListRenderer::render(const Domain::Tile *current_tile,
                               const Domain::Position &current_pos,
                               int &selected_index, bool &spawn_selected,
                               bool &creature_selected) {
  if (!current_tile)
    return;

  // Calculate total items to display
  int const ground_count = current_tile->hasGround() ? 1 : 0;
  size_t const item_count = current_tile->getItemCount();
  int const total_items = ground_count + static_cast<int>(item_count);

  // Use ImGuiListClipper for performance with large item stacks (e.g. trash
  // tiles)
  ImGuiListClipper clipper;
  // Hoist items vector access outside the loop for performance
  const auto &items = current_tile->getItems();
  clipper.Begin(total_items);

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      if (ground_count > 0 && i == 0) {
        // Render Ground (always index 0 if present)
        const Domain::Item *ground = current_tile->getGround();
        renderItemRow(ground, ground->getType(), i, true, current_pos,
                      selected_index, spawn_selected, creature_selected);
      } else {
        // Render Stacked Items
        // Map display index 'i' to item vector index
        int const item_idx = i - ground_count;

        if (item_idx >= 0 && static_cast<size_t>(item_idx) < items.size()) {
          const Domain::Item *item = items[item_idx].get();
          renderItemRow(item, item->getType(), i, false, current_pos,
                        selected_index, spawn_selected, creature_selected);
        }
      }
    }
  }
}

void ItemsListRenderer::renderItemRow(const Domain::Item *item,
                                      const Domain::ItemType *type,
                                      int display_index, bool is_ground,
                                      const Domain::Position &current_pos,
                                      int &selected_index, bool &spawn_selected,
                                      bool &creature_selected) {
  ImGui::PushID(item);

  bool image_rendered = false;
  if (sprite_manager_ && type) {
    if (auto *texture = sprite_manager_->getItemCompositor().getCompositedItemTexture(type)) {
      ImGui::Image((void *)(intptr_t)texture->id(), ImVec2(32, 32));
      image_rendered = true;
    }
  }

  // Ensure consistent row height for clipper by adding placeholder if no image
  if (!image_rendered) {
    ImGui::Dummy(ImVec2(32, 32));
  }

  ImGui::SameLine();

  std::string label =
      type && !type->name.empty()
          ? std::format("{} ({})", type->name, item->getServerId())
          : std::format("Item {}", item->getServerId());

  if (is_ground) {
    label += " [GND]";
  } else if (item->getCount() > 1) {
    label += std::format(" x{}", item->getCount());
  }

  bool const is_selected = (selected_index == display_index);
  if (ImGui::Selectable(label.c_str(), is_selected)) {
    selected_index = display_index;
    spawn_selected = false;
    creature_selected = false;
  }

  // Drag source
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    ImGui::SetDragDropPayload("BROWSE_TILE_ITEM", &display_index, sizeof(int));
    ImGui::Text("%s", label.c_str());
    ImGui::EndDragDropSource();
  }

  // Drop target
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("BROWSE_TILE_ITEM")) {
      int const source_index = *(const int *)payload->Data;
      if (source_index != display_index) {
        handleItemDragDrop(source_index, display_index, current_pos,
                           selected_index);
      }
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::PopID();
}

ToolbarAction ItemsListRenderer::renderToolbar(
    const Domain::Tile *current_tile, const Domain::Position &current_pos,
    int &selected_index, bool spawn_selected, bool creature_selected) {
  ToolbarAction action = ToolbarAction::None;

  if (!current_tile)
    return action;

  bool const has_selection = selected_index >= 0;
  bool const can_execute = has_selection && session_ && map_;

  // Add spacing before buttons
  ImGui::Spacing();

  // Move up button
  ImGui::BeginDisabled(!can_execute ||
                       selected_index <= (current_tile->hasGround() ? 1 : 0));
  if (ImGui::Button(ICON_FA_ARROW_UP)) {
    bool const has_ground = current_tile->hasGround();
    int const src_idx = has_ground ? selected_index - 1 : selected_index;
    int const dst_idx = has_ground ? selected_index - 2 : selected_index - 1;

    if (src_idx >= 0 && dst_idx >= 0) {
      moveItem(current_pos, src_idx, dst_idx, selected_index);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Move item up in stack");
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  // Move down button
  size_t max_index = current_tile->getItemCount();
  if (current_tile->hasGround())
    max_index++;

  ImGui::BeginDisabled(!can_execute ||
                       selected_index >= static_cast<int>(max_index) - 1);
  if (ImGui::Button(ICON_FA_ARROW_DOWN)) {
    bool const has_ground = current_tile->hasGround();
    int const src_idx = has_ground ? selected_index - 1 : selected_index;
    int const dst_idx = has_ground ? selected_index : selected_index + 1;

    if (src_idx >= 0 && dst_idx >= 0) {
      moveItem(current_pos, src_idx, dst_idx, selected_index);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Move item down in stack");
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  // Delete button
  bool const can_delete_item = has_selection && session_ && map_;
  bool const can_delete_spawn =
      spawn_selected && session_ && map_ && current_tile->hasSpawn();
  bool const can_delete_creature =
      creature_selected && session_ && map_ && current_tile->hasCreature();
  bool const can_delete = can_delete_item || can_delete_spawn || can_delete_creature;

  ImGui::BeginDisabled(!can_delete);
  if (ImGui::Button(ICON_FA_TRASH)) {
    if (can_delete_spawn) {
      // Delegate spawn deletion to parent
      action = ToolbarAction::DeleteSpawn;
    } else if (can_delete_creature) {
      // Delegate creature deletion to parent
      action = ToolbarAction::DeleteCreature;
    } else if (can_delete_item) {
      // Handle item deletion internally
      handleDelete(selected_index, current_tile, current_pos, selected_index);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (spawn_selected) {
      ImGui::SetTooltip("Delete spawn from tile");
    } else if (creature_selected) {
      ImGui::SetTooltip("Delete creature from tile");
    } else {
      ImGui::SetTooltip("Delete item from tile");
    }
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Separator();

  return action;
}

void ItemsListRenderer::handleItemDragDrop(int source_index, int display_index,
                                           const Domain::Position &current_pos,
                                           int &selected_index) {
  if (session_ && map_) {
    // Need to re-fetch context to map indices (requires hasGround check)
    const Domain::Tile *tile = map_->getTile(current_pos);
    if (tile) {
      bool const has_ground = tile->hasGround();
      int const src_item_idx = has_ground ? source_index - 1 : source_index;
      int const dst_item_idx = has_ground ? display_index - 1 : display_index;

      if (src_item_idx >= 0 && dst_item_idx >= 0) {
        swapItems(current_pos, src_item_idx, dst_item_idx);
      }
      selected_index = display_index;
    }
  }
}

void ItemsListRenderer::checkDragOutDeletion(
    const Domain::Tile *current_tile, const Domain::Position &current_pos,
    int &selected_index) {
  // This is called from the parent context (Browse Tile window), so
  // GetWindowPos returns parent bounds.
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
      ImGui::GetDragDropPayload() != nullptr) {
    const ImGuiPayload *payload = ImGui::GetDragDropPayload();
    if (payload->IsDataType("BROWSE_TILE_ITEM")) {
      ImVec2 const mouse_pos = ImGui::GetMousePos();
      ImVec2 const window_pos = ImGui::GetWindowPos();
      ImVec2 const window_size = ImGui::GetWindowSize();

      bool const outside = (mouse_pos.x < window_pos.x ||
                      mouse_pos.x > window_pos.x + window_size.x ||
                      mouse_pos.y < window_pos.y ||
                      mouse_pos.y > window_pos.y + window_size.y);

      if (outside) {
        int const source_index = *(const int *)payload->Data;
        handleDelete(source_index, current_tile, current_pos, selected_index);
      }
    }
  }
}

void ItemsListRenderer::handleDelete(int source_index,
                                     const Domain::Tile *current_tile,
                                     const Domain::Position &current_pos,
                                     int &selected_index) {
  if (session_ && map_) {
    bool const is_ground = current_tile->hasGround() && source_index == 0;
    auto &hm = session_->getHistoryManager();
    auto &selection = session_->getSelectionService();
    hm.beginOperation("Delete item", Domain::History::ActionType::Other, &selection);
    hm.recordTileBefore(current_pos, current_tile);

    // Get mutable tile for operation
    Domain::Tile *mutable_tile = map_->getTile(current_pos);
    if (mutable_tile) {
      if (is_ground) {
        mutable_tile->setGround(nullptr);
      } else {
        size_t const item_idx =
            current_tile->hasGround() ? source_index - 1 : source_index;
        mutable_tile->removeItem(item_idx);
      }
    }
    hm.endOperation(map_, &selection);
    selected_index = -1;
  }
}

void ItemsListRenderer::swapItems(const Domain::Position &pos, int src_idx,
                                  int dst_idx) {
  auto &hm = session_->getHistoryManager();
  auto &selection = session_->getSelectionService();
  Domain::Tile *mutable_tile = map_->getTile(pos);
  if (!mutable_tile)
    return;

  hm.beginOperation("Reorder item", Domain::History::ActionType::Other, &selection);
  hm.recordTileBefore(pos, mutable_tile);
  mutable_tile->swapItems(static_cast<size_t>(src_idx),
                          static_cast<size_t>(dst_idx));
  hm.endOperation(map_, &selection);
}

void ItemsListRenderer::moveItem(const Domain::Position &pos, int src_idx,
                                 int dst_idx, int &selected_index) {
  swapItems(pos, src_idx, dst_idx);
  // Follow the moved item
  if (src_idx < dst_idx)
    selected_index++;
  else
    selected_index--;
}

} // namespace MapEditor::UI::BrowseTile
