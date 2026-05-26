#include "StartupDialog.h"
#include "Utils/FormatUtils.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <nfd.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace UI {

void StartupDialog::initialize(Services::ClientVersionRegistry *registry,
                               Services::ConfigService *config) {
  registry_ = registry;
  config_ = config;

  // Initialize standalone dialogs (DRY - no duplicate rendering)
  new_map_dialog_.initialize(registry);
  new_map_dialog_.setOnConfirm([this](const NewMapPanel::State& config) {
    pending_result_.action = Action::NewMapConfirmed;
    pending_result_.new_map_config = config;
  });

  // Wire up extracted panel components
  available_clients_panel_.setRegistry(registry);
  available_clients_panel_.setSelectionCallback([this](uint32_t index) {
    pending_result_.action = Action::SelectClient;
    pending_result_.selected_client_index = index;
  });

  recent_maps_panel_.setSelectionCallback(
      [this](int index, const RecentMapEntry &entry) {
        selected_recent_index_ = index;
        pending_result_.action = Action::SelectRecentMap;
        pending_result_.selected_path = entry.path;
        pending_result_.selected_index = index;
      });

  recent_maps_panel_.setDoubleClickCallback(
      [this](int index, const RecentMapEntry &entry) {
        if (load_enabled_) {
          pending_result_.action = Action::LoadMap;
        }
      });

  spdlog::info("StartupDialog initialized");
}

StartupDialog::Result StartupDialog::consumeResult() {
  Result result = pending_result_;
  pending_result_ = {};
  return result;
}

void StartupDialog::setSelectedMapInfo(const SelectedMapInfo &info) {
  selected_map_info_ = info;
}

void StartupDialog::setClientInfo(const ClientInfo &info) {
  client_info_ = info;
}

void StartupDialog::setSignatureMismatch(bool mismatch,
                                         const std::string &message) {
  signature_mismatch_ = mismatch;
  signature_mismatch_message_ = message;
}

void StartupDialog::setIgnoreSignatures(bool ignore) {
  ignore_signatures_ = ignore;
}

void StartupDialog::setLoadEnabled(bool enabled) { load_enabled_ = enabled; }

void StartupDialog::setClientNotConfigured(bool not_configured) {
  client_not_configured_ = not_configured;
}

void StartupDialog::setSelectedIndex(int index) {
  selected_recent_index_ = index;
}

void StartupDialog::render(const std::vector<RecentMapEntry> &recent_maps,
                           uint32_t matched_client_index) {
  ImGuiIO &io = ImGui::GetIO();

  // Centered modal window with default size 1280x720
  const ImVec2 default_size(1280.0f, 720.0f);
  ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(default_size, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(900, 550),
                                      ImVec2(FLT_MAX, FLT_MAX));

  // Dark modern styling
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.12f, 0.14f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.16f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

  if (ImGui::Begin("Tibia Map Editor - Startup", nullptr, flags)) {
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 content_region = ImGui::GetContentRegionAvail();

    // ===== HEADER SECTION =====
    renderHeader();

    ImGui::Spacing();
    ImGui::Spacing();

    // ===== MAIN CONTENT AREA =====
    float footer_height = 60.0f;
    float header_offset = 70.0f;
    float main_height = content_region.y - footer_height - header_offset;

    // Layout dimensions
    float sidebar_width = 180.0f;
    float spacing = 12.0f;
    float remaining_width = content_region.x - sidebar_width - spacing;
    float panel_width = (remaining_width - spacing * 3) / 4.0f;

    // ===== LEFT SIDEBAR =====
    ImGui::BeginChild("##Sidebar", ImVec2(sidebar_width, main_height), true);
    renderSidebar();
    ImGui::EndChild();

    ImGui::SameLine(0, spacing);

    // ===== PANEL 1: Recent Maps List =====
    ImGui::BeginChild("##RecentMaps", ImVec2(panel_width, main_height), true);
    renderRecentMapsPanel(recent_maps);
    ImGui::EndChild();

    ImGui::SameLine(0, spacing);

    // ===== PANEL 2: Selected Map Information =====
    ImGui::BeginChild("##MapInfo", ImVec2(panel_width, main_height), true);
    renderSelectedMapPanel();
    ImGui::EndChild();

    ImGui::SameLine(0, spacing);

    // ===== PANEL 3: Client Information =====
    ImGui::BeginChild("##ClientInfo", ImVec2(panel_width, main_height), true);
    renderClientInfoPanel();
    ImGui::EndChild();

    ImGui::SameLine(0, spacing);

    // ===== PANEL 4: Latest Used Clients =====
    ImGui::BeginChild("##RecentClients", ImVec2(panel_width, main_height),
                      true);
    renderRecentClientsPanel(matched_client_index);
    ImGui::EndChild();

    // ===== FOOTER SECTION =====
    ImGui::Spacing();
    renderFooter();
  }
  ImGui::End();

  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(2);

  // ===== RENDER MODALS (using standalone dialogs - DRY) =====
  
  // Trigger dialogs if flag is set
  if (show_new_map_modal_) {
    new_map_dialog_.show();
    show_new_map_modal_ = false;
  }
  
  // Render dialogs (handles their own popup modals)
  new_map_dialog_.render();

  // Render sub-dialogs if open
  if (client_config_dialog_.isOpen()) {
    client_config_dialog_.render();
  }
}

void StartupDialog::renderHeader() {
  // Uniform button size for all buttons in the UI
  const ImVec2 kUniformButtonSize(150.0f, 36.0f);
  const float header_height = 60.0f;

  // Card-style header background (no scrolling)
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.18f, 1.0f));
  ImGui::BeginChild("##HeaderCard", ImVec2(0, header_height), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  // Title with larger text
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::SetWindowFontScale(1.4f);
  ImGui::Text("Tibia Map Editor");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();

  // Subtitle
  ImGui::TextColored(
      ImVec4(0.6f, 0.65f, 0.7f, 1.0f),
      "Welcome! Start a new project or continue where you left off.");

  // Header buttons on the right (Preferences + Client Configuration)
  float button_spacing = 8.0f;
  float total_buttons_width = kUniformButtonSize.x * 2 + button_spacing;
  float right_padding = 8.0f;
  float button_y = (header_height - kUniformButtonSize.y) / 2.0f;

  // Position first button
  ImGui::SameLine(ImGui::GetWindowWidth() - total_buttons_width -
                  right_padding);
  ImGui::SetCursorPosY(button_y);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.35f, 0.38f, 0.42f, 1.0f));
  if (ImGui::Button(ICON_FA_GEAR " Preferences", kUniformButtonSize)) {
    pending_result_.action = Action::Preferences;
  }
  ImGui::PopStyleColor(2);

  // Position second button at same Y level
  ImGui::SameLine(0, button_spacing);
  ImGui::SetCursorPosY(button_y);

  // Client Configuration button (moved from footer)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.35f, 0.38f, 0.42f, 1.0f));
  if (ImGui::Button(ICON_FA_SLIDERS " Client Config", kUniformButtonSize)) {
    pending_result_.action = Action::ClientConfiguration;
  }
  ImGui::PopStyleColor(2);

  ImGui::EndChild();
  ImGui::PopStyleColor(); // ChildBg
}

void StartupDialog::renderSidebar() {
  // Uniform button size for all buttons in the UI
  const ImVec2 kUniformButtonSize(150.0f, 36.0f);

  ImGui::Spacing();

  // New Map button (primary action - blue)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.70f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.25f, 0.55f, 0.80f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.15f, 0.40f, 0.65f, 1.0f));
  if (ImGui::Button(ICON_FA_FILE " New map", kUniformButtonSize)) {
    pending_result_.action = Action::NewMap;
  }
  ImGui::PopStyleColor(3);

  ImGui::Spacing();
  ImGui::Spacing();

  // Browse Map button (secondary - dark gray)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.28f, 0.30f, 0.34f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.16f, 0.18f, 0.20f, 1.0f));
  if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse Map", kUniformButtonSize)) {
    pending_result_.action = Action::BrowseMap;
  }
  ImGui::PopStyleColor(3);

  ImGui::Spacing();
  ImGui::Spacing();

  // Browse .sec MAP button (secondary - dark gray)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.28f, 0.30f, 0.34f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.16f, 0.18f, 0.20f, 1.0f));
  if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS " Browse .sec",
                    kUniformButtonSize)) {
    pending_result_.action = Action::BrowseSecMap;
  }
  ImGui::PopStyleColor(3);
}

void StartupDialog::renderRecentMapsPanel(
    const std::vector<RecentMapEntry> &entries) {
  // Sync state to panel
  recent_maps_panel_.setSelectedIndex(selected_recent_index_);
  recent_maps_panel_.setLoadEnabled(load_enabled_);

  // Delegate rendering to extracted component
  recent_maps_panel_.render(entries);

  // Sync state back
  selected_recent_index_ = recent_maps_panel_.getSelectedIndex();
}

void StartupDialog::renderSelectedMapPanel() {
  // Sync state and delegate to extracted component
  selected_map_panel_.setMapInfo(selected_map_info_);
  selected_map_panel_.render();
}

void StartupDialog::renderClientInfoPanel() {
  // Sync state and delegate to extracted component
  client_info_panel_.setClientInfo(client_info_);
  client_info_panel_.setMapInfo(selected_map_info_);
  client_info_panel_.setSignatureMismatch(signature_mismatch_,
                                          signature_mismatch_message_);
  client_info_panel_.setClientNotConfigured(client_not_configured_);
  client_info_panel_.render();
}

void StartupDialog::renderRecentClientsPanel(
    uint32_t selected_client_index) {
  available_clients_panel_.setSelectedIndex(selected_client_index);
  available_clients_panel_.render();
}

void StartupDialog::renderFooter() {
  ImVec2 region = ImGui::GetContentRegionAvail();

  // Uniform button size for all buttons in the UI
  const ImVec2 kUniformButtonSize(150.0f, 36.0f);
  const float footer_height = 50.0f;

  // Footer background
  ImGui::BeginChild("##Footer", ImVec2(0, footer_height), false,
                    ImGuiWindowFlags_NoScrollbar);

  float button_y = (footer_height - kUniformButtonSize.y) / 2.0f;

  // Exit button (left)
  ImGui::SetCursorPosY(button_y);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.35f, 0.38f, 0.42f, 1.0f));
  if (ImGui::Button(ICON_FA_POWER_OFF " Exit", kUniformButtonSize)) {
    pending_result_.action = Action::Exit;
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  ImGui::SetCursorPosY(button_y + 8.0f);
  ImGui::TextColored(ImVec4(0.5f, 0.52f, 0.55f, 1.0f), "Version 2.4.1");

  // Right side buttons: Ignore Signatures toggle + Load Map
  float button_spacing = 8.0f;
  float right_buttons_width = kUniformButtonSize.x * 2 + button_spacing;
  float right_padding = 8.0f;

  ImGui::SameLine(region.x - right_buttons_width - right_padding);
  ImGui::SetCursorPosY(button_y);

  // Ignore signatures toggle button (converted from checkbox)
  if (ignore_signatures_) {
    // Active state - more prominent color
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.50f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.40f, 0.55f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.30f, 0.45f, 0.60f, 1.0f));
  } else {
    // Inactive state - standard gray
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.35f, 0.38f, 0.42f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.20f, 0.22f, 0.26f, 1.0f));
  }

  const char *sig_label = ignore_signatures_ ? ICON_FA_CHECK " Ignore Sigs"
                                             : ICON_FA_XMARK " Ignore Sigs";
  if (ImGui::Button(sig_label, kUniformButtonSize)) {
    ignore_signatures_ = !ignore_signatures_;
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine(0, button_spacing);

  // Load Map button (primary - blue)
  bool can_load = load_enabled_ || ignore_signatures_;

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.85f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.25f, 0.55f, 0.90f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.15f, 0.45f, 0.80f, 1.0f));

  if (!can_load) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button(ICON_FA_UPLOAD " Load Map", kUniformButtonSize) &&
      can_load) {
    pending_result_.action = Action::LoadMap;
  }

  if (!can_load) {
    ImGui::PopStyleVar();
  }

  ImGui::PopStyleColor(3);

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
