#include "UI/Dialogs/ClientConfiguration/ClientConfigurationDialog.h"
#include "UI/Core/Theme.h"
#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"
#include "Presentation/Dialogs/ClientConfigurationController.h"
#include "Domain/ClientVersion.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <imgui.h>
#include <vector>

namespace MapEditor {
namespace UI {

namespace {

void pushCompactStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6.0f, 4.0f));
}
void popCompactStyle() { ImGui::PopStyleVar(5); }

constexpr ImVec4 kBlueAccent   = SemanticColors::INFO;
constexpr ImVec4 kBlueHover    = SemanticColors::INFO;
constexpr ImVec4 kBlueActive   = SemanticColors::INFO;
constexpr ImVec4 kRedDelete    = SemanticColors::DANGER;
constexpr ImVec4 kRedHover     = SemanticColors::DANGER;
constexpr ImVec4 kGreenStatus  = SemanticColors::SAVED;
constexpr ImVec4 kTextOffWhite = SemanticColors::HEADER_TEXT;
constexpr ImVec4 kTextMuted    = SemanticColors::MUTED;

} // namespace

ClientConfigurationDialog::ClientConfigurationDialog()
    : editor_(std::make_unique<ClientPropertyEditor>()) {}
ClientConfigurationDialog::~ClientConfigurationDialog() = default;

void ClientConfigurationDialog::open(
    Presentation::ClientConfigurationController& controller,
    Services::ClientVersionRegistry& registry,
    Services::ConfigService& config) {
    controller_ = &controller;
    controller_->open(registry, config);
    controller_->setChangeCallback([this]() { controller_->runAssetDetection(); });
    editor_->setRegistry(controller_->registry());
    editor_->setController(controller_);
    is_open_ = true;
}

void ClientConfigurationDialog::close() { is_open_ = false; }

bool ClientConfigurationDialog::render() {
    if (!is_open_ || !controller_) return false;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 win_size(1180, 800);
    ImVec2 win_pos((io.DisplaySize.x - win_size.x) * 0.5f,
                   (io.DisplaySize.y - win_size.y) * 0.5f);
    ImGui::SetNextWindowPos(win_pos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(win_size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900, 500), ImVec2(FLT_MAX, FLT_MAX));

    pushCompactStyle();

    bool open = true;
    ImGui::Begin("Client Configuration", &open,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (controller_->hasDirty()) ImGui::OpenPopup("Unsaved Changes");
        else is_open_ = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
        if (controller_->saveAll()) controller_->populateVersionData();
        else ImGui::OpenPopup("Validation Error");
    }

    renderTitleBar();
    renderToolbar();
    ImGui::Separator();
    renderBody();

    if (ImGui::BeginPopupModal("Validation Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Please fix the following before saving:");
        ImGui::TextWrapped("%s", controller_->validationError().c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
    popCompactStyle();

    if (!open) is_open_ = false;
    return is_open_;
}

void ClientConfigurationDialog::renderTitleBar() {
    ImGui::BeginChild("##titlebar", ImVec2(0, 32.0f), ImGuiChildFlags_None);
    ImGui::SetCursorPosY(6);
    ImGui::SetCursorPosX(10);
    ImGui::TextColored(kTextOffWhite, "Client Configuration");
    ImGui::EndChild();
}

void ClientConfigurationDialog::renderToolbar() {
    ImGui::BeginChild("##toolbar", ImVec2(0, 48.0f), ImGuiChildFlags_None);
    ImGui::SetCursorPosY(6);

    ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBlueActive);
    if (ImGui::Button(ICON_FA_PLUS "  Add", ImVec2(100, 32))) controller_->addClient();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_COPY "  Duplicate", ImVec2(120, 32))) controller_->duplicateClient();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, kRedDelete);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kRedHover);
    if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(100, 32)))
        controller_->deleteClient(controller_->activeVersion());
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
}

void ClientConfigurationDialog::renderBody() {
    float body_h = ImGui::GetContentRegionAvail().y - 56.0f;
    ImGui::BeginChild("##body", ImVec2(0, body_h), ImGuiChildFlags_None);
    renderLeftSidebar();
    ImGui::SameLine(0, 8);
    renderRightPanel();
    ImGui::EndChild();
    renderFooterStatus();
}

void ClientConfigurationDialog::renderLeftSidebar() {
    ImGui::BeginChild("##leftbar", ImVec2(350, 0), ImGuiChildFlags_Borders);

    ImGui::Separator();

    ImGui::Columns(4, "##ver_cols4", false);
    ImGui::SetColumnWidth(0, 40); ImGui::SetColumnWidth(1, 150); ImGui::SetColumnWidth(2, 60); ImGui::SetColumnWidth(3, 60);
    ImGui::TextColored(kTextMuted, "Index");   ImGui::NextColumn();
    ImGui::TextColored(kTextMuted, "Name");    ImGui::NextColumn();
    ImGui::TextColored(kTextMuted, "Version"); ImGui::NextColumn();
    ImGui::TextColored(kTextMuted, "Type");    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();

    renderVersionList();

    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search_bottom", ICON_FA_MAGNIFYING_GLASS " Search...",
                                 search_buf_, sizeof(search_buf_))) {
        controller_->setSearchFilter(search_buf_);
    }

    ImGui::EndChild();
}

void ClientConfigurationDialog::renderVersionList() {
    auto* reg = controller_->registry();
    auto& filtered = controller_->filteredVersions();

    ImGui::BeginChild("##verlist", ImVec2(0, -34), ImGuiChildFlags_None);
    if (filtered.empty()) { ImGui::TextDisabled("  No matching clients"); }

    for (uint32_t ver_num : filtered) {
        if (!reg) continue;
        auto* cv = reg->getVersion(ver_num);
        if (!cv) continue;

        bool sel = (controller_->activeVersion() == ver_num);
        bool dirty = cv->isDirty();
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Header, kBlueAccent);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kBlueHover);
        } else if (dirty) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.50f, 0.42f, 0.14f, 1.0f));
        }

        ImGui::PushID(static_cast<int>(ver_num));
        ImGui::Columns(4, "##verlist_cols4", false);
        ImGui::SetColumnWidth(0, 40); ImGui::SetColumnWidth(1, 150); ImGui::SetColumnWidth(2, 60); ImGui::SetColumnWidth(3, 60);

        ImGui::TextColored(kTextMuted, "%u", cv->getIndex());
        ImGui::NextColumn();

        std::string name_display = cv->getName();
        if (dirty) name_display += " *";
        if (ImGui::Selectable(name_display.c_str(), sel,
                              ImGuiSelectableFlags_AllowDoubleClick |
                                  ImGuiSelectableFlags_SpanAllColumns)) {
            controller_->selectClient(ver_num);
        }

        if (ImGui::BeginPopupContextItem(std::format("ctx_{}", ver_num).c_str())) {
            if (ImGui::MenuItem("Duplicate")) controller_->duplicateClient(ver_num);
            if (ImGui::MenuItem("Delete")) controller_->deleteClient(ver_num);
            ImGui::EndPopup();
        }

        ImGui::NextColumn();
        ImGui::TextColored(kTextMuted, "%u", cv->getVersion());

        ImGui::NextColumn();
        const char* type_str = "???";
        switch (cv->getDataSource()) {
            case Domain::ItemDataSource::OTB: type_str = "OTB"; break;
            case Domain::ItemDataSource::SRV: type_str = "SRV"; break;
            case Domain::ItemDataSource::DAT: type_str = "DAT"; break;
        }
        ImGui::TextColored(kTextMuted, "%s", type_str);

        ImGui::Columns(1);
        ImGui::PopID();

        if (sel) ImGui::PopStyleColor(2);
        else if (dirty) ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

void ClientConfigurationDialog::renderRightPanel() {
    ImGui::BeginChild("##rightpanel", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (controller_->activeVersion() == 0) {
        ImGui::SetCursorPosY(40);
        ImGui::TextColored(kTextMuted,
                           "   Select a client version from the list on the left "
                           "to edit its identity, files, compatibility, and "
                           "feature flags.");
    } else {
        editor_->setActiveVersion(controller_->activeVersion());
        editor_->render();
    }

    ImGui::EndChild();
}

void ClientConfigurationDialog::renderFooterStatus() {
    ImGui::BeginChild("##footer", ImVec2(0, 56.0f), ImGuiChildFlags_None);
    ImGui::SetCursorPosY(8);

    if (controller_->activeVersion() != 0) {
        editor_->setActiveVersion(controller_->activeVersion());
        editor_->renderStatusBar();
    }

    float btn_x = ImGui::GetWindowWidth() - 310;
    ImGui::SameLine(btn_x);

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Apply", ImVec2(90, 28))) {
        if (controller_->saveAll()) controller_->populateVersionData();
        else ImGui::OpenPopup("Validation Error");
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover);
    if (ImGui::Button(ICON_FA_CHECK " Save", ImVec2(90, 28))) {
        if (controller_->saveAll()) { controller_->populateVersionData(); is_open_ = false; }
        else ImGui::OpenPopup("Validation Error");
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_XMARK " Close", ImVec2(90, 28))) {
        if (controller_->hasDirty()) {
            ImGui::OpenPopup("Unsaved Changes");
        } else {
            is_open_ = false;
        }
    }

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION " You have unsaved changes.");
        ImGui::TextUnformatted("Discard them and close?");
        ImGui::Spacing();
        if (ImGui::Button("Discard && Close", ImVec2(140, 0))) {
            controller_->discardChanges(); is_open_ = false; ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
