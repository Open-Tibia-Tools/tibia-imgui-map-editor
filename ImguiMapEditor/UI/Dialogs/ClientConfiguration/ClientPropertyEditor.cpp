#include "UI/Dialogs/ClientConfiguration/ClientPropertyEditor.h"
#include "Presentation/Dialogs/ClientConfigurationController.h"
#include "Services/ClientVersionRegistry.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <nfd.hpp>
#include <sstream>
#include <vector>

namespace MapEditor {
namespace UI {

namespace {

const char* getParserName(uint32_t version) {
    if (version >= 1057) return "DAT Parser 10.57+";
    if (version >= 1050) return "DAT Parser 10.50+";
    if (version >= 1010) return "DAT Parser 10.10+";
    if (version >= 960)  return "DAT Parser 9.60+";
    if (version >= 860)  return "DAT Parser 8.60";
    if (version >= 780)  return "DAT Parser 7.80";
    if (version >= 755)  return "DAT Parser 7.55";
    if (version >= 740)  return "DAT Parser 7.40";
    if (version >= 710)  return "DAT Parser 7.10";
    return "Unknown Parser";
}

ImVec4 blend(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

constexpr ImVec4 kRed       = ImVec4(0.6f, 0.15f, 0.15f, 1.0f);
constexpr ImVec4 kYellow    = ImVec4(0.6f, 0.5f, 0.0f, 1.0f);
constexpr ImVec4 kGreen     = ImVec4(0.15f, 0.55f, 0.15f, 1.0f);
constexpr ImVec4 kTextMuted = ImVec4(0.67f, 0.70f, 0.75f, 1.0f);
constexpr ImVec4 kGreenStatus = ImVec4(0.43f, 0.82f, 0.43f, 1.0f);
constexpr ImVec4 kBlueAccent  = ImVec4(0.19f, 0.44f, 0.84f, 1.0f);
constexpr ImVec4 kBlueHover   = ImVec4(0.25f, 0.50f, 0.92f, 1.0f);

float labelColumn() { return 195.0f; }

class ScopedPropertyColor {
public:
    ScopedPropertyColor(const char* prop,
                        const std::unordered_map<std::string, Domain::PropertyVisualState>* st)
        : states_(st), name_(prop) {
        if (!states_) return;
        auto it = states_->find(name_);
        if (it == states_->end()) return;
        switch (it->second) {
        case Domain::PropertyVisualState::Pending:    tint_ = kYellow; break;
        case Domain::PropertyVisualState::Undetected: tint_ = kRed;    break;
        case Domain::PropertyVisualState::Saved:      tint_ = kGreen;  break;
        default: return;
        }
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
            blend(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), tint_, 0.30f));
        pushed_ = true;
    }
    ~ScopedPropertyColor() { if (pushed_) ImGui::PopStyleColor(); }
private:
    const std::unordered_map<std::string, Domain::PropertyVisualState>* states_ = nullptr;
    const char* name_ = nullptr;
    ImVec4 tint_{1,1,1,0};
    bool pushed_ = false;
};

// Helper: render a text field with label + ScopedPropertyColor
template<typename OnChange>
void renderTextField(const char* label, const char* id, const char* stateKey,
                     char* buf, size_t bufSize, const auto& states,
                     const char* tooltip, OnChange&& onChange) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(-1);
    {
        ScopedPropertyColor sc(stateKey, &states);
        if (ImGui::InputText(id, buf, bufSize) && buf[0])
            onChange();
    }
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopItemWidth();
}

// Helper: render a path field with label + InputText + folder browse button
template<typename OnBrowse, typename OnChange>
void renderPathField(const char* label, const char* inputId, const char* stateKey,
                     char* buf, size_t bufSize, const auto& states,
                     const char* tooltip, bool disabled, bool readOnly,
                     const char* browseId, OnBrowse&& onBrowse, OnChange&& onChange) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
    {
        if (disabled) ImGui::BeginDisabled();
        ScopedPropertyColor sc(stateKey, &states);
        auto flags = readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
        if (ImGui::InputText(inputId, buf, bufSize, flags) && !readOnly)
            onChange();
        if (disabled) ImGui::EndDisabled();
    }
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    {
        if (disabled) ImGui::BeginDisabled();
        if (ImGui::Button(browseId, ImVec2(28, 0))) onBrowse();
        if (disabled) ImGui::EndDisabled();
    }
}

} // namespace

void ClientPropertyEditor::render() {
    if (active_version_ == 0 || !registry_ || !controller_) return;
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;

    renderIdentitySection();
    if (cv->getVersion() > 0) {
        renderFilesSection();
        renderCompatibilitySection();
        renderSignaturesSection();
        renderFeaturesSection();
    }
}

void ClientPropertyEditor::renderStatusBar() {
    if (!registry_ || !controller_) return;
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SetCursorPosX(12);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);

    bool dirty = cv->isDirty();
    ImU32 dot = dirty ? IM_COL32(231, 209, 46, 255) : IM_COL32(110, 209, 110, 255);
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 7), 5, dot);

    ImGui::SetCursorPosX(24);
    ImGui::TextColored(kTextMuted, "Status: ");
    ImGui::SameLine(0, 0);
    ImVec4 col = dirty ? ImVec4(0.9f, 0.7f, 0.2f, 1.0f) : kGreenStatus;
    ImGui::TextColored(col, "%s", cv->getName().c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextColored(kTextMuted, "  |  ");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(col, "%s", dirty ? "Modified" : "Saved");
}

void ClientPropertyEditor::renderIdentitySection() {
    if (!ImGui::TreeNodeEx("Identity", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto* cv = registry_->getVersion(active_version_);
    if (!cv) { ImGui::TreePop(); return; }
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    ImGui::TextUnformatted("Item Data Source:");
    ImGui::SameLine(labelColumn());

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

    auto dataSrcButton = [&](const char* name, int idx, Domain::ItemDataSource ds) {
        bool active = (*eb.dataSourceIdx == idx);
        if (active) { ImGui::PushStyleColor(ImGuiCol_Button, kBlueAccent); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueHover); }
        if (ImGui::Button(name, ImVec2(42, 0))) {
            *eb.dataSourceIdx = idx;
            if (cv) { cv->setDataSource(ds); cv->markDirty(); }
        }
        if (active) ImGui::PopStyleColor(2);
    };

    dataSrcButton("OTB", 0, Domain::ItemDataSource::OTB); ImGui::SameLine();
    dataSrcButton("SRV", 1, Domain::ItemDataSource::SRV); ImGui::SameLine();
    dataSrcButton("DAT", 2, Domain::ItemDataSource::DAT);
    ImGui::PopStyleVar();

    ImGui::TextUnformatted("Client Version:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(140);
    // Show templates from clients.json (read-only reference)
    const auto& templates = registry_->getTemplates();
    std::vector<std::string> ver_labels;
    for (const auto& tpl : templates) {
        ver_labels.push_back(std::format("{} ({})", tpl.name, tpl.version));
    }
    int ver_sel = -1;
    for (size_t i = 0; i < templates.size(); ++i)
        if (static_cast<int>(templates[i].version) == *eb.versionInt) { ver_sel = static_cast<int>(i); break; }
    static bool open_custom_popup = false;
    bool version_empty = (cv->getVersion() == 0);
    if (version_empty) {
        float pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) + 1.0f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.5f, 0.0f, 0.3f + pulse * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.5f, 0.0f, 0.5f + pulse * 0.5f));
    }
    if (ImGui::BeginCombo("##versioncombo", ver_sel >= 0 ? ver_labels[ver_sel].c_str() : version_empty ? "Select version..." : "")) {
        for (size_t i = 0; i < templates.size(); ++i) {
            if (ImGui::Selectable(ver_labels[i].c_str(), ver_sel == static_cast<int>(i))) {
                const auto& tpl = templates[i];
                *eb.versionInt = static_cast<int>(tpl.version);
                if (cv) {
                    cv->setVersion(tpl.version);
                    cv->setDataSource(tpl.data_source);
                    *eb.dataSourceIdx = static_cast<int>(tpl.data_source);
                    cv->setOtbVersion(tpl.otb_id);
                    *eb.otbId = static_cast<int>(tpl.otb_id);
                    cv->setOtbMajor(tpl.otb_major);
                    *eb.otbMajor = static_cast<int>(tpl.otb_major);
                    cv->setMapVersionsSupported(tpl.otbm_versions);
                    std::string otbm_str;
                    for (size_t j = 0; j < tpl.otbm_versions.size(); ++j) {
                        if (j > 0) otbm_str += ", ";
                        otbm_str += std::to_string(tpl.otbm_versions[j]);
                    }
                    std::strncpy(eb.otbmVersions, otbm_str.c_str(), eb.otbmVersionsSize - 1);
                    eb.otbmVersions[eb.otbmVersionsSize - 1] = '\0';
                    cv->setDatSignature(tpl.dat_signature);
                    std::snprintf(eb.datSig, eb.datSigSize, "%08X", tpl.dat_signature);
                    cv->setSprSignature(tpl.spr_signature);
                    std::snprintf(eb.sprSig, eb.sprSigSize, "%08X", tpl.spr_signature);
                    cv->setTransparent(tpl.transparency);
                    cv->setExtended(tpl.extended);
                    cv->setFrameDurations(tpl.frame_durations);
                    cv->setFrameGroups(tpl.frame_groups);
                    *eb.transparent = tpl.transparency;
                    *eb.extended = tpl.extended;
                    *eb.frameDurations = tpl.frame_durations;
                    *eb.frameGroups = tpl.frame_groups;
                    auto dd = (std::filesystem::path("data") / std::to_string(tpl.version)).string();
                    cv->setDataDirectory(dd);
                    std::strncpy(eb.dataDir, dd.c_str(), eb.dataDirSize - 1);
                    eb.dataDir[eb.dataDirSize - 1] = '\0';
                    cv->markDirty();
                }
            }
            if (ver_sel == static_cast<int>(i)) ImGui::SetItemDefaultFocus();
        }
        ImGui::Separator();
        if (ImGui::Selectable("<custom>"))
            open_custom_popup = true;
        ImGui::EndCombo();
    }
    if (version_empty) ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    if (open_custom_popup) {
        open_custom_popup = false;
        ImGui::OpenPopup("Custom Version");
    }

    // Custom version modal
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Custom Version", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char new_name_buf[64] = {};
        static char new_ver_buf[16] = {};
        ImGui::TextUnformatted("Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##customname", new_name_buf, sizeof(new_name_buf));
        ImGui::TextUnformatted("Version:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##customver", new_ver_buf, sizeof(new_ver_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0))) {
            int v = std::atoi(new_ver_buf);
            if (v > 0 && new_name_buf[0]) {
                controller_->addClient();
                auto eb = controller_->getEditableBuffers();
                std::strncpy(eb.name, new_name_buf, eb.nameSize - 1);
                eb.name[eb.nameSize - 1] = '\0';
                *eb.versionInt = v;
                if (auto* cv = controller_->registry()->getVersion(controller_->activeVersion())) {
                    controller_->syncToClient(*cv);
                }
                new_name_buf[0] = '\0';
                new_ver_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            new_name_buf[0] = '\0';
            new_ver_buf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Parser:");
    ImGui::SameLine();
    ImGui::TextColored(kGreenStatus, "%s", getParserName(static_cast<uint32_t>(*eb.versionInt)));

    renderTextField("Name:", "##name", "name", eb.name, eb.nameSize, states, nullptr,
                    [&]() { cv->setName(eb.name); cv->markDirty(); controller_->setPropertyState(active_version_, "name", Domain::PropertyVisualState::Pending); });
    renderTextField("Description:", "##desc", "desc", eb.desc, eb.descSize, states, nullptr,
                    [&]() { cv->setDescription(eb.desc); cv->markDirty(); controller_->setPropertyState(active_version_, "desc", Domain::PropertyVisualState::Pending); });

    ImGui::TreePop();
}

// --- Files Section (split into sub-functions) ---

void ClientPropertyEditor::renderFilesSection() {
    if (!ImGui::TreeNodeEx("Files && Paths", ImGuiTreeNodeFlags_DefaultOpen)) return;
    renderClientPathField();
    renderDataDirectoryField();
    renderMetadataFileField();
    renderSpritesFileField();
    renderItemsDatabaseField();
    ImGui::TreePop();
}

void ClientPropertyEditor::renderClientPathField() {
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    bool pathStale = std::filesystem::path(eb.metadata).parent_path().string() != std::string(eb.clientPath);
    bool path_empty = (eb.clientPath[0] == '\0');
    bool client_version_set = (cv->getVersion() > 0);

    ImGui::TextUnformatted("Client Path:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 36);
    {
        if (pathStale) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        } else if (path_empty && client_version_set) {
            float pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) + 1.0f) * 0.5f;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.5f, 0.0f, 0.3f + pulse * 0.5f));
        }
        ScopedPropertyColor sc("clientPath", &states);
        if (ImGui::InputText("##clientpath", eb.clientPath, eb.clientPathSize)) {
            cv->setClientPath(eb.clientPath);
            cv->markDirty();
            controller_->setPropertyState(active_version_, "clientPath", Domain::PropertyVisualState::Pending);
        }
        if (pathStale) ImGui::PopStyleColor(2);
        else if (path_empty && client_version_set) ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        if (pathStale)
            ImGui::SetTooltip("File path diverged from client folder.");
        else if (path_empty && client_version_set)
            ImGui::SetTooltip("Browse to your Tibia client folder");
        else
            ImGui::SetTooltip("Folder containing Tibia.dat and Tibia.spr");
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (path_empty) {
        float pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) + 1.0f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.2f, 0.5f + pulse * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
    }
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##browse", ImVec2(28, 0))) {
        NFD::UniquePath outPath;
        if (NFD::PickFolder(outPath) == NFD_OKAY) {
            std::filesystem::path cp(outPath.get());
            std::strncpy(eb.clientPath, cp.string().c_str(), eb.clientPathSize - 1);
            eb.clientPath[eb.clientPathSize - 1] = '\0';
            cv->setClientPath(cp);
            cv->markDirty();
            controller_->autoDetectFromClientPath(cp);
            controller_->setPropertyState(active_version_, "clientPath", Domain::PropertyVisualState::Pending);
            controller_->setPropertyState(active_version_, "metadataFile", Domain::PropertyVisualState::Pending);
            controller_->setPropertyState(active_version_, "spritesFile", Domain::PropertyVisualState::Pending);
            controller_->setPropertyState(active_version_, "itemsDb", Domain::PropertyVisualState::Pending);
        }
    }
    if (path_empty) ImGui::PopStyleColor(2);
}

void ClientPropertyEditor::renderDataDirectoryField() {
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    renderPathField("Data Directory:", "##datadir", "dataDir",
                    eb.dataDir, eb.dataDirSize, states,
                    "Editor data subdirectory for this client version. Defaults to data/<version>",
                    false, false, ICON_FA_FOLDER_OPEN "##ddbrowse",
                    [&]() {
                        NFD::UniquePath outPath;
                        if (NFD::PickFolder(outPath) == NFD_OKAY) {
                            std::strncpy(eb.dataDir, outPath.get(), eb.dataDirSize - 1);
                            eb.dataDir[eb.dataDirSize - 1] = '\0';
                            cv->setDataDirectory(outPath.get());
                            cv->markDirty();
                            controller_->setPropertyState(active_version_, "dataDir", Domain::PropertyVisualState::Pending);
                        }
                    },
                    [&]() {
                        cv->setDataDirectory(eb.dataDir);
                        cv->markDirty();
                        controller_->setPropertyState(active_version_, "dataDir", Domain::PropertyVisualState::Pending);
                    });
}

void ClientPropertyEditor::renderMetadataFileField() {
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    renderPathField("Metadata File:", "##metadata", "metadataFile",
                    eb.metadata, eb.metadataSize, states,
                    "Full absolute path to the DAT (.dat) file",
                    false, false, ICON_FA_FOLDER_OPEN "##metabrowse",
                    [&]() {
                        NFD::UniquePath outPath;
                        if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
                            std::filesystem::path sp(outPath.get());
                            std::strncpy(eb.metadata, sp.string().c_str(), eb.metadataSize - 1);
                            eb.metadata[eb.metadataSize - 1] = '\0';
                            cv->setMetadataFile(sp.string());
                            cv->markDirty();
                            controller_->setPropertyState(active_version_, "metadataFile", Domain::PropertyVisualState::Pending);
                            controller_->invalidateClientPath();
                        }
                    },
                    [&]() {
                        cv->setMetadataFile(eb.metadata);
                        cv->markDirty();
                        controller_->setPropertyState(active_version_, "metadataFile", Domain::PropertyVisualState::Pending);
                        if (!controller_->isAutoFilling()) controller_->invalidateClientPath();
                    });
}

void ClientPropertyEditor::renderSpritesFileField() {
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    renderPathField("Sprites File:", "##sprites", "spritesFile",
                    eb.sprites, eb.spritesSize, states,
                    "Full absolute path to the SPR (.spr) file",
                    false, false, ICON_FA_FOLDER_OPEN "##sprbrowse",
                    [&]() {
                        NFD::UniquePath outPath;
                        if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
                            std::filesystem::path sp(outPath.get());
                            std::strncpy(eb.sprites, sp.string().c_str(), eb.spritesSize - 1);
                            eb.sprites[eb.spritesSize - 1] = '\0';
                            cv->setSpritesFile(sp.string());
                            cv->markDirty();
                            controller_->setPropertyState(active_version_, "spritesFile", Domain::PropertyVisualState::Pending);
                            controller_->invalidateClientPath();
                        }
                    },
                    [&]() {
                        cv->setSpritesFile(eb.sprites);
                        cv->markDirty();
                        controller_->setPropertyState(active_version_, "spritesFile", Domain::PropertyVisualState::Pending);
                        if (!controller_->isAutoFilling()) controller_->invalidateClientPath();
                    });
}

void ClientPropertyEditor::renderItemsDatabaseField() {
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);
    bool is_dat = (*eb.dataSourceIdx == 2);

    renderPathField("Items Database:", "##itemsdb", "itemsDb",
                    eb.itemsDb, eb.itemsDbSize, states,
                    "Full absolute path to a custom items database (leave empty to use default)",
                    is_dat, is_dat, ICON_FA_FOLDER_OPEN "##itemsdbbrowse",
                    [&]() {
                        if (is_dat) return;
                        NFD::UniquePath outPath;
                        if (NFD::OpenDialog(outPath, nullptr, 0) == NFD_OKAY) {
                            std::string sel(outPath.get());
                            std::strncpy(eb.itemsDb, sel.c_str(), eb.itemsDbSize - 1);
                            eb.itemsDb[eb.itemsDbSize - 1] = '\0';
                            cv->setCustomItemsDbPath(std::filesystem::path(sel));
                            cv->markDirty();
                            controller_->setPropertyState(active_version_, "itemsDb", Domain::PropertyVisualState::Pending);
                            controller_->invalidateClientPath();
                        }
                    },
                    [&]() {
                        cv->setCustomItemsDbPath(std::filesystem::path(eb.itemsDb));
                        cv->markDirty();
                        controller_->setPropertyState(active_version_, "itemsDb", Domain::PropertyVisualState::Pending);
                        if (!controller_->isAutoFilling()) controller_->invalidateClientPath();
                    });
}

void ClientPropertyEditor::renderCompatibilitySection() {
    if (!ImGui::TreeNodeEx("Compatibility", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    char otb_id_buf[16], otb_major_buf[16];
    std::snprintf(otb_id_buf, sizeof(otb_id_buf), "%d", *eb.otbId);
    std::snprintf(otb_major_buf, sizeof(otb_major_buf), "%d", *eb.otbMajor);

    ImGui::TextUnformatted("OTB ID:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(80);
    {
        ScopedPropertyColor sc("otbId", &states);
        if (ImGui::InputText("##otbid", otb_id_buf, sizeof(otb_id_buf), ImGuiInputTextFlags_CharsDecimal)) {
            *eb.otbId = std::max(0, std::atoi(otb_id_buf));
            if (auto* cv = registry_->getVersion(active_version_)) {
                cv->markDirty(); cv->setOtbVersion(static_cast<uint32_t>(*eb.otbId));
                controller_->setPropertyState(active_version_, "otbId", Domain::PropertyVisualState::Pending);
            }
        }
    }
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Major:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(80);
    {
        ScopedPropertyColor sc("otbMajor", &states);
        if (ImGui::InputText("##otbmajor", otb_major_buf, sizeof(otb_major_buf), ImGuiInputTextFlags_CharsDecimal)) {
            *eb.otbMajor = std::max(0, std::atoi(otb_major_buf));
            if (auto* cv = registry_->getVersion(active_version_)) {
                cv->markDirty(); cv->setOtbMajor(static_cast<uint32_t>(*eb.otbMajor));
                controller_->setPropertyState(active_version_, "otbMajor", Domain::PropertyVisualState::Pending);
            }
        }
    }
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("OTBM:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(80);
    {
        ScopedPropertyColor sc("otbmVersions", &states);
        if (ImGui::InputText("##otbmver", eb.otbmVersions, eb.otbmVersionsSize)) {
            if (auto* cv = registry_->getVersion(active_version_)) {
                cv->markDirty();
                std::vector<uint32_t> otbm;
                std::istringstream iss(eb.otbmVersions); std::string tok;
                while (std::getline(iss, tok, ',')) {
                    try {
                        uint32_t v = static_cast<uint32_t>(std::stoul(tok));
                        if (v >= 1 && v <= 4) otbm.push_back(v);
                    } catch (...) {}
                }
                cv->setMapVersionsSupported(otbm);
                controller_->setPropertyState(active_version_, "otbmVersions", Domain::PropertyVisualState::Pending);
            }
        }
    }
    ImGui::PopItemWidth();

    ImGui::TreePop();
}

void ClientPropertyEditor::renderSignaturesSection() {
    if (!ImGui::TreeNodeEx("Signatures", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);

    ImGui::TextUnformatted("DAT Signature:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(-1);
    {
        ScopedPropertyColor sc("datSignature", &states);
        if (ImGui::InputText("##datsig", eb.datSig, eb.datSigSize, ImGuiInputTextFlags_CharsHexadecimal)) {
            if (auto* cv = registry_->getVersion(active_version_)) {
                uint32_t sig = 0; std::istringstream(eb.datSig) >> std::hex >> sig;
                cv->setDatSignature(sig); cv->markDirty();
                controller_->setPropertyState(active_version_, "datSignature", Domain::PropertyVisualState::Pending);
            }
        }
    }
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("SPR Signature:");
    ImGui::SameLine(labelColumn());
    ImGui::PushItemWidth(-1);
    {
        ScopedPropertyColor sc("sprSignature", &states);
        if (ImGui::InputText("##sprsig", eb.sprSig, eb.sprSigSize, ImGuiInputTextFlags_CharsHexadecimal)) {
            if (auto* cv = registry_->getVersion(active_version_)) {
                uint32_t sig = 0; std::istringstream(eb.sprSig) >> std::hex >> sig;
                cv->setSprSignature(sig); cv->markDirty();
                controller_->setPropertyState(active_version_, "sprSignature", Domain::PropertyVisualState::Pending);
            }
        }
    }
    ImGui::PopItemWidth();

    ImGui::TreePop();
}

void ClientPropertyEditor::renderFeaturesSection() {
    if (!ImGui::TreeNodeEx("Features", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto eb = controller_->getEditableBuffers();
    const auto& states = controller_->getStates(active_version_);
    float half = ImGui::GetContentRegionAvail().x * 0.48f;

    auto boolBox = [&](const char* label, const char* prop, bool* val, auto setter) {
        ScopedPropertyColor sc(prop, &states);
        if (ImGui::Checkbox(label, val)) {
            if (auto* cv = registry_->getVersion(active_version_)) {
                setter(cv, *val); cv->markDirty();
                controller_->setPropertyState(active_version_, prop, Domain::PropertyVisualState::Pending);
            }
        }
    };

    boolBox("Transparency", "transparency", eb.transparent,
            [](auto* c, bool v) { c->setTransparent(v); });
    ImGui::SameLine(0, 20);
    ImGui::SetCursorPosX(labelColumn() + half);
    boolBox("Extended", "extended", eb.extended,
            [](auto* c, bool v) { c->setExtended(v); });

    boolBox("Frame Durations", "frameDurations", eb.frameDurations,
            [](auto* c, bool v) { c->setFrameDurations(v); });
    ImGui::SameLine(0, 20);
    ImGui::SetCursorPosX(labelColumn() + half);
    boolBox("Frame Groups", "frameGroups", eb.frameGroups,
            [](auto* c, bool v) { c->setFrameGroups(v); });

    {
        ScopedPropertyColor sc("default", &states);
        if (ImGui::Checkbox("Set as Default", eb.isDefault)) {
            if (auto* cv = registry_->getVersion(active_version_)) {
                cv->setDefault(*eb.isDefault);
                if (*eb.isDefault && registry_) {
                    registry_->setDefaultVersion(cv->getIndex());
                } else if (registry_ && registry_->getDefaultVersion() == cv->getIndex()) {
                    registry_->setDefaultVersion(0);
                }
                cv->markDirty();
            }
        }
    }

    ImGui::TreePop();
}

} // namespace UI
} // namespace MapEditor
