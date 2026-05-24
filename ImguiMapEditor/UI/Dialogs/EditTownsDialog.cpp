#include "EditTownsDialog.h"
#include "Application/MapTabManager.h"
#include "Application/EditorSession.h"
#include "Domain/MapInstance.h"
#include <imgui.h>
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <ranges>

namespace MapEditor {
namespace UI {

static constexpr ImVec4 kModifiedYellow{0.50f, 0.42f, 0.14f, 1.0f};
static constexpr ImVec4 kDangerRed{0.70f, 0.18f, 0.18f, 1.0f};
static constexpr ImVec4 kDangerRedHover{0.80f, 0.25f, 0.25f, 1.0f};
static constexpr ImVec4 kDangerRedActive{0.60f, 0.12f, 0.12f, 1.0f};
static constexpr ImVec4 kGoToBlue{0.18f, 0.40f, 0.65f, 1.0f};
static constexpr ImVec4 kGoToBlueHover{0.22f, 0.48f, 0.75f, 1.0f};
static constexpr ImVec4 kGoToBlueActive{0.14f, 0.35f, 0.58f, 1.0f};

static float bounceOffset() {
    return std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) * 3.0f;
}

static void drawOutlinedIcon(ImDrawList* dl, ImVec2 center, float fontSize,
                             ImU32 fillColor, const char* icon) {
    const ImU32 kOutline = IM_COL32(0, 0, 0, 220);
    float o = 1.5f;
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x - o, center.y), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x + o, center.y), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x, center.y - o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x, center.y + o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x + o, center.y + o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x - o, center.y - o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x + o, center.y - o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(center.x - o, center.y + o), kOutline, icon);
    dl->AddText(ImGui::GetFont(), fontSize, center, fillColor, icon);
}

void EditTownsDialog::show() {
    is_open_ = true;
    is_picking_position_ = false;
    refreshFromActiveMap();
}

EditTownsDialog::Result EditTownsDialog::render() {
    Result result = Result::None;

    if (!is_open_) return result;

    auto* session = tab_manager_ ? tab_manager_->getActiveSession() : nullptr;
    auto* active_map = session ? session->getMap() : nullptr;

    if (active_map != last_map_) {
        refreshFromActiveMap();
    }

    if (active_map && selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
        if (selected_index_ != last_selected_index_) {
            last_selected_index_ = selected_index_;
            minimap_renderer_.setZoomLevel(-1);
            last_minimap_pos_ = Domain::Position{-1, -1, -1};
        }
        const auto& pos = towns_[selected_index_].temple_position;
        updateMinimapView(pos);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(940, 580), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Edit Towns###EditTownsDialog", &is_open_,
                     ImGuiWindowFlags_NoCollapse)) {

        const float panel_w = 220.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float center_w = std::max(0.0f, avail_w - panel_w * 2.0f - spacing * 2.0f);
        const float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        const float content_h = ImGui::GetContentRegionAvail().y - footer_h;

        bool has_selection = selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size());

        // === LEFT PANEL: Towns ===
        ImGui::BeginChild("##towns_panel", ImVec2(panel_w, content_h), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Towns");

        ImGui::BeginChild("##towns_list", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y), ImGuiChildFlags_None);
        for (size_t i : std::views::iota(size_t(0), towns_.size())) {
            const auto& town = towns_[i];
            ImGui::PushID(static_cast<int>(town.id));

            bool is_modified = modified_town_ids_.contains(town.id);

            char label[256];
            snprintf(label, sizeof(label), "%u. %s%s", town.id, town.name.c_str(),
                     is_modified ? " *" : "");

            bool is_selected = (static_cast<int>(i) == selected_index_);

            if (is_modified) {
                ImGui::PushStyleColor(ImGuiCol_Header, kModifiedYellow);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.58f, 0.50f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.37f, 0.10f, 1.0f));
            }

            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                selected_index_ = static_cast<int>(i);
                syncEditBuffers();
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (on_go_to_) {
                        on_go_to_(town.temple_position);
                    }
                }
            }

            if (is_modified) {
                ImGui::PopStyleColor(3);
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        if (ImGui::Button(ICON_FA_PLUS " Add")) {
            TownEntry new_town;
            new_town.id = getNextTownId();
            new_town.name = "New Town";
            new_town.temple_position = Domain::Position(0, 0, 7);
            new_town.is_new = true;

            towns_.push_back(new_town);
            modified_town_ids_.insert(new_town.id);
            selected_index_ = static_cast<int>(towns_.size()) - 1;
            syncEditBuffers();
        }

        ImGui::SameLine();

        bool can_remove = canRemoveSelectedTown();
        ImGui::BeginDisabled(!can_remove);
        if (ImGui::Button(ICON_FA_TRASH " Remove")) {
            if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
                show_delete_confirm_ = true;
                ImGui::OpenPopup("Remove Town?");
            }
        }
        ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!can_remove && selected_index_ >= 0) {
                ImGui::SetTooltip("Cannot remove town with associated houses");
            }
        }

        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Remove Town?", &show_delete_confirm_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
                ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION " Are you sure you want to remove:");
                ImGui::TextDisabled("ID %u: %s", towns_[selected_index_].id, towns_[selected_index_].name.c_str());
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

                if (ImGui::Button(ICON_FA_TRASH " Yes, Remove", ImVec2(120, 0))) {
                    uint32_t removed_id = towns_[selected_index_].id;
                    towns_.erase(towns_.begin() + selected_index_);
                    modified_town_ids_.erase(removed_id);

                    if (selected_index_ >= static_cast<int>(towns_.size())) {
                        selected_index_ = static_cast<int>(towns_.size()) - 1;
                    }
                    syncEditBuffers();
                    show_delete_confirm_ = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    show_delete_confirm_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
            } else {
                show_delete_confirm_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::SameLine(0.0f, spacing);

        // === CENTER PANEL: Map Preview ===
        ImGui::BeginChild("##preview_panel", ImVec2(center_w, content_h), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Map Preview");

        float preview_h = ImGui::GetContentRegionAvail().y;
        int preview_w = static_cast<int>(ImGui::GetContentRegionAvail().x);
        int preview_h_int = static_cast<int>(preview_h);

        if (minimap_source_ && preview_w > 0 && preview_h_int > 0) {
            minimap_renderer_.update(preview_w, preview_h_int);
            auto tex_id = minimap_renderer_.getTextureId();
            if (tex_id != 0) {
                ImVec2 img_cursor = ImGui::GetCursorScreenPos();
                ImGui::Image((void*)(intptr_t)tex_id, ImVec2(static_cast<float>(preview_w), preview_h));

                // Scroll wheel zoom
                if (ImGui::IsItemHovered()) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (io.MouseWheel > 0) {
                        minimap_renderer_.zoomIn();
                    } else if (io.MouseWheel < 0) {
                        minimap_renderer_.zoomOut();
                    }
                }

                // Animated map pin at temple position
                if (has_selection) {
                    const auto& tpos = towns_[selected_index_].temple_position;
                    int pin_x = 0, pin_y = 0;
                    minimap_renderer_.worldToScreen(tpos.x, tpos.y, pin_x, pin_y);

                    float screen_x = img_cursor.x + static_cast<float>(pin_x);
                    float screen_y = img_cursor.y + static_cast<float>(pin_y) + bounceOffset();

                    float fontSize = ImGui::GetFontSize() * 2.5f;
                    ImVec2 pin_center(screen_x - fontSize * 0.33f, screen_y - fontSize * 0.75f);

                    drawOutlinedIcon(ImGui::GetWindowDrawList(), pin_center, fontSize,
                                     IM_COL32(255, 210, 20, 255), ICON_FA_LOCATION_DOT);
                }
            } else {
                ImGui::TextDisabled("Loading...");
            }
        } else {
            ImGui::TextDisabled("No towns on this map");
        }

        ImGui::EndChild();
        ImGui::SameLine(0.0f, spacing);

        // === RIGHT PANEL: Town Properties ===
        ImGui::BeginChild("##properties_panel", ImVec2(panel_w, content_h), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Town Properties");

        bool selected_modified = has_selection && modified_town_ids_.contains(towns_[selected_index_].id);

        ImGui::BeginDisabled(!has_selection);

        ImGui::Spacing();

        ImGui::TextUnformatted("Name:");
        if (selected_modified) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, kModifiedYellow);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##TownName", name_buffer_, sizeof(name_buffer_))) {
            if (has_selection) {
                towns_[selected_index_].name = name_buffer_;
                markModified(towns_[selected_index_].id);
            }
        }
        if (selected_modified) {
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        ImGui::TextUnformatted("ID:");
        if (has_selection) {
            ImGui::TextDisabled("%u", towns_[selected_index_].id);
        } else {
            ImGui::TextDisabled("-");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextUnformatted("Temple Position:");

        ImGui::TextUnformatted("X:");
        if (selected_modified) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, kModifiedYellow);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##TempleX", &temple_x_, 0, 0)) {
            if (has_selection) {
                towns_[selected_index_].temple_position.x = temple_x_;
                markModified(towns_[selected_index_].id);
            }
        }
        if (selected_modified) {
            ImGui::PopStyleColor();
        }

        ImGui::TextUnformatted("Y:");
        if (selected_modified) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, kModifiedYellow);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##TempleY", &temple_y_, 0, 0)) {
            if (has_selection) {
                towns_[selected_index_].temple_position.y = temple_y_;
                markModified(towns_[selected_index_].id);
            }
        }
        if (selected_modified) {
            ImGui::PopStyleColor();
        }

        ImGui::TextUnformatted("Z:");
        if (selected_modified) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, kModifiedYellow);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##TempleZ", &temple_z_, 0, 0)) {
            temple_z_ = std::clamp(temple_z_, 0, 15);
            if (has_selection) {
                towns_[selected_index_].temple_position.z = static_cast<int16_t>(temple_z_);
                markModified(towns_[selected_index_].id);
            }
        }
        if (selected_modified) {
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, kDangerRed);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kDangerRedHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kDangerRedActive);
        if (ImGui::Button(ICON_FA_CROSSHAIRS " Change Temple Position", ImVec2(-1, 0))) {
            if (on_pick_position_ && on_pick_position_()) {
                is_picking_position_ = true;
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click on map to set temple position");
        }

        ImGui::PushStyleColor(ImGuiCol_Button, kGoToBlue);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kGoToBlueHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGoToBlueActive);
        if (ImGui::Button(ICON_FA_LOCATION_DOT " Go To Position", ImVec2(-1, 0))) {
            if (has_selection && on_go_to_) {
                on_go_to_(towns_[selected_index_].temple_position);
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move camera to temple position");
        }

        if (is_picking_position_) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                              ICON_FA_CROSSHAIRS " Click on map to select...");
        }

        ImGui::EndDisabled();
        ImGui::EndChild();

        // === FOOTER ===
        float button_w = 110.0f;
        float total_buttons_w = button_w * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        float offset_x = ImGui::GetContentRegionAvail().x - total_buttons_w;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

        if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(button_w, 0))) {
            result = Result::Cancelled;
            is_open_ = false;
            is_picking_position_ = false;
        }

        ImGui::SameLine();

        float flash_age = static_cast<float>(ImGui::GetTime()) - apply_flash_time_;
        bool flash_active = flash_age < 1.0f && flash_age > 0.0f;

        if (flash_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.65f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.50f, 0.12f, 1.0f));
        }

        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Apply", ImVec2(button_w, 0))) {
            applyChangesToMap();
            syncEditBuffers();
            apply_flash_time_ = static_cast<float>(ImGui::GetTime());
        }

        if (flash_active) {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.40f, 0.65f, 1.0f));

        if (ImGui::Button(ICON_FA_CHECK " OK", ImVec2(button_w, 0))) {
            applyChangesToMap();
            result = Result::Applied;
            is_open_ = false;
            is_picking_position_ = false;
        }

        ImGui::PopStyleColor(3);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            result = Result::Cancelled;
            is_open_ = false;
            is_picking_position_ = false;
        }
    }

    ImGui::End();

    if (is_picking_position_) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImVec2 mp = ImGui::GetMousePos();
        float sz = ImGui::GetFontSize() * 2.0f;
        fg->AddText(ImGui::GetFont(), sz, ImVec2(mp.x - sz * 0.5f, mp.y - sz * 0.5f),
                    IM_COL32(200, 200, 200, 255), ICON_FA_LOCATION_CROSSHAIRS);
    }

    // Draw temple position marker on main map viewport
    if (is_open_ && selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size()) && tile_to_screen_) {
        const auto& tpos = towns_[selected_index_].temple_position;
        glm::vec2 screen_pos = tile_to_screen_(tpos);
        if (screen_pos.x >= 0 && screen_pos.y >= 0) {
            float fontSize = ImGui::GetFontSize() * 2.5f;
            ImVec2 center(screen_pos.x - fontSize * 0.4f, screen_pos.y - fontSize * 0.75f + bounceOffset());

            drawOutlinedIcon(ImGui::GetForegroundDrawList(), center, fontSize,
                             IM_COL32(255, 210, 20, 255), ICON_FA_LOCATION_DOT);
        }
    }

    if (!is_open_) {
        is_picking_position_ = false;
        if (result == Result::None) {
            result = Result::Cancelled;
        }
    }

    return result;
}

void EditTownsDialog::setPickedPosition(const Domain::Position& pos) {
    if (!is_picking_position_ || selected_index_ < 0 ||
        selected_index_ >= static_cast<int>(towns_.size())) {
        return;
    }

    towns_[selected_index_].temple_position = pos;
    temple_x_ = pos.x;
    temple_y_ = pos.y;
    temple_z_ = pos.z;
    markModified(towns_[selected_index_].id);
    is_picking_position_ = false;
}

void EditTownsDialog::refreshFromActiveMap() {
    auto* session = tab_manager_ ? tab_manager_->getActiveSession() : nullptr;
    map_ = session ? session->getMap() : nullptr;
    last_map_ = map_;

    auto* client_data = (session && session->getDocument())
        ? session->getDocument()->getClientData() : nullptr;

    if (map_) {
        if (client_data) {
            minimap_source_ = std::make_unique<Rendering::ChunkedMapMinimapSource>(map_, client_data);
            minimap_renderer_.setDataSource(minimap_source_.get());
            minimap_renderer_.setZoomLevel(-1);
            last_minimap_pos_ = Domain::Position{-1, -1, -1};
        } else {
            minimap_source_.reset();
            minimap_renderer_.setDataSource(nullptr);
        }
        loadTownsFromMap();
    } else {
        minimap_source_.reset();
        minimap_renderer_.setDataSource(nullptr);
        towns_.clear();
        selected_index_ = -1;
    }
}

void EditTownsDialog::loadTownsFromMap() {
    towns_.clear();
    modified_town_ids_.clear();
    selected_index_ = -1;

    if (!map_) return;

    const auto& map_towns = map_->getTowns();
    towns_.reserve(map_towns.size());

    std::ranges::transform(map_towns, std::back_inserter(towns_), [](const auto& town) {
        return TownEntry{town.id, town.name, town.temple_position, false};
    });

    std::ranges::sort(towns_, {}, &TownEntry::id);

    if (!towns_.empty()) {
        selected_index_ = 0;
        syncEditBuffers();
    }
}

void EditTownsDialog::applyChangesToMap() {
    if (!map_) return;

    auto current_ids_view = map_->getTowns() | std::views::transform([](const auto& t) { return t.id; });

    std::vector<uint32_t> ids_to_remove;
    std::ranges::copy_if(current_ids_view, std::back_inserter(ids_to_remove), [this](uint32_t old_id) {
        return std::ranges::find(towns_, old_id, &TownEntry::id) == towns_.end();
    });

    for (uint32_t id : ids_to_remove) {
        map_->removeTown(id);
    }

    for (const auto& entry : towns_) {
        auto* existing = map_->getTown(entry.id);
        if (existing) {
            map_->updateTown(entry.id, entry.name, entry.temple_position);
        } else {
            map_->addTown(entry.id, entry.name, entry.temple_position);
        }
    }

    modified_town_ids_.clear();
}

void EditTownsDialog::syncEditBuffers() {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
        const auto& town = towns_[selected_index_];
        strncpy(name_buffer_, town.name.c_str(), sizeof(name_buffer_) - 1);
        name_buffer_[sizeof(name_buffer_) - 1] = '\0';
        temple_x_ = town.temple_position.x;
        temple_y_ = town.temple_position.y;
        temple_z_ = town.temple_position.z;
    } else {
        name_buffer_[0] = '\0';
        temple_x_ = 0;
        temple_y_ = 0;
        temple_z_ = 7;
    }
}

void EditTownsDialog::markModified(uint32_t town_id) {
    modified_town_ids_.insert(town_id);
}

bool EditTownsDialog::canRemoveSelectedTown() const {
    if (selected_index_ < 0 || selected_index_ >= static_cast<int>(towns_.size())) {
        return false;
    }

    if (!map_) return true;

    uint32_t town_id = towns_[selected_index_].id;
    return !map_->hasTownWithHouses(town_id);
}

uint32_t EditTownsDialog::getNextTownId() const {
    if (towns_.empty()) return 1;
    auto max_it = std::ranges::max_element(towns_, {}, &TownEntry::id);
    return max_it->id + 1;
}

void EditTownsDialog::updateMinimapView(const Domain::Position& pos) {
    if (pos == last_minimap_pos_) return;
    last_minimap_pos_ = pos;

    minimap_renderer_.setViewCenter(pos.x, pos.y);
    minimap_renderer_.setFloor(pos.z);
}

} // namespace UI
} // namespace MapEditor
