#include "ClientConfigurationController.h"
#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include "Services/ClientAssetDetector.h"
#include "Services/ClientVersionPersistence.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <sstream>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Presentation {

namespace {

constexpr const char* kAutoProps[] = {
    "metadataFile", "spritesFile", "datSignature", "sprSignature",
    "transparency", "extended",  "frameDurations", "frameGroups",
};
constexpr size_t kAutoCount = sizeof(kAutoProps) / sizeof(kAutoProps[0]);

bool isAutoProp(const char* name) {
    for (size_t i = 0; i < kAutoCount; ++i)
        if (std::strcmp(kAutoProps[i], name) == 0) return true;
    return false;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string dataSourceToString(Domain::ItemDataSource ds) {
    switch (ds) {
        case Domain::ItemDataSource::OTB: return "OTB";
        case Domain::ItemDataSource::SRV: return "SRV";
        case Domain::ItemDataSource::DAT: return "DAT";
        default: return "Unknown";
    }
}

} // namespace

// === Session lifecycle ===

void ClientConfigurationController::open(Services::ClientVersionRegistry& registry,
                                         Services::ConfigService& config) {
    registry_ = &registry;
    config_ = &config;
    is_open_ = true;
    active_version_ = 0;
    search_buf_[0] = '\0';
    search_filter_.clear();
    pending_deleted_.clear();
    pending_created_.clear();

    auto config_path = std::filesystem::current_path() / "data" / "clients_saved.json";
    registry_->setConfigPath(config_path);

    auto [versions, default_ver] = Services::ClientVersionPersistence::loadFromJson(config_path);
    for (auto& [index, cv] : versions) {
        if (!registry_->getVersion(index))
            registry_->addClient(cv);
    }
    if (!versions.empty())
        registry_->setNextIndex(versions.rbegin()->first);

    if (registry_->getDefaultVersion() > 0)
        active_version_ = registry_->getDefaultVersion();
    else if (!registry_->getVersionsMap().empty())
        active_version_ = registry_->getVersionsMap().begin()->first;

    populateVersionData();

    if (active_version_ != 0) {
        if (auto* cv = registry_->getVersion(active_version_)) {
            cv->backup();
            syncFromClient(*cv);
        }
    }
}

void ClientConfigurationController::close() { is_open_ = false; }

// === Selection ===

void ClientConfigurationController::selectClient(uint32_t version) {
    if (version == active_version_) return;
    active_version_ = version;
    if (version == 0) return;
    if (auto* cv = registry_->getVersion(version)) {
        cv->backup();
        syncFromClient(*cv);
    }
}

// === CRUD ===

void ClientConfigurationController::addClient() {
    std::string new_name = "New Client";
    int counter = 1;
    for (bool unique = false; !unique;) {
        unique = true;
        for (const auto& [num, ver] : registry_->getVersionsMap()) {
            if (ver.getName() == new_name) {
                new_name = "New Client " + std::to_string(counter++);
                unique = false;
                break;
            }
        }
    }

    uint32_t new_index = registry_->nextIndex();
    Domain::ClientVersion cv(new_index, 0, new_name, 0);
    cv.setDataSource(Domain::ItemDataSource::OTB);
    cv.markDirty();

    registry_->addClient(cv);
    registry_->backupVersion(new_index);
    pending_created_.insert(new_index);
    populateVersionData();
    selectClient(new_index);
}

void ClientConfigurationController::duplicateClient() {
    duplicateClient(active_version_);
}

void ClientConfigurationController::duplicateClient(uint32_t from_index) {
    if (from_index == 0) return;
    auto* src = registry_->getVersion(from_index);
    if (!src) return;

    std::string new_name = src->getName() + " (Copy)";
    uint32_t new_index = registry_->nextIndex();

    Domain::ClientVersion clone(new_index, src->getVersion(), new_name, src->getOtbVersion());
    clone.setDescription(src->getDescription());
    clone.setDataDirectory(src->getDataDirectory());
    clone.setClientPath(src->getClientPath());
    clone.setOtbMajor(src->getOtbMajor());
    clone.setMapVersionsSupported(src->getMapVersionsSupported());
    clone.setDatSignature(src->getDatSignature());
    clone.setSprSignature(src->getSprSignature());
    clone.setMetadataFile(src->getMetadataFile());
    clone.setSpritesFile(src->getSpritesFile());
    clone.setTransparent(src->isTransparent());
    clone.setExtended(src->isExtended());
    clone.setFrameDurations(src->hasFrameDurations());
    clone.setFrameGroups(src->hasFrameGroups());
    clone.setDataSource(src->getDataSource());
    clone.setDatFormat(src->getDatFormat());
    clone.setCustomItemsDbPath(src->getCustomItemsDbPath());
    clone.markDirty();

    registry_->addClient(clone);
    registry_->backupVersion(new_index);
    pending_created_.insert(new_index);
    populateVersionData();
    selectClient(new_index);
}

void ClientConfigurationController::deleteClient(uint32_t version) {
    if (version == 0) return;
    pending_deleted_.insert(version);
    clearPropertyStates(version);
    populateVersionData();
    if (active_version_ == version) {
        active_version_ = 0;
        if (!filtered_versions_.empty())
            selectClient(filtered_versions_[0]);
    }
}

// === Persistence ===

bool ClientConfigurationController::saveAll() {
    if (!registry_) return false;
    if (!validateBeforeSave()) return false;

    auto versions = registry_->getVersionsMap();
    uint32_t default_ver = registry_->getDefaultVersion();

    for (auto index : pending_deleted_) {
        versions.erase(index);
        if (default_ver == index) default_ver = 0;
    }

    if (!Services::ClientVersionPersistence::saveToJson(
            registry_->getConfigPath(), versions, default_ver)) {
        spdlog::error("Failed to save clients_saved.json");
        return false;
    }

    for (auto index : pending_deleted_)
        registry_->removeClient(index);

    for (const auto& [num, _] : registry_->getVersionsMap())
        if (auto* cv = registry_->getVersion(num)) cv->clearDirty();
    for (const auto& [num, _] : registry_->getVersionsMap())
        markAllPendingAsSaved(num);

    pending_deleted_.clear();
    pending_created_.clear();

    auto old_path = std::filesystem::current_path() / "data" / "configured_clients.json";
    if (std::filesystem::exists(old_path))
        std::filesystem::remove(old_path);

    return true;
}

void ClientConfigurationController::discardChanges() {
    for (auto id : pending_created_)
        registry_->removeClient(id);
    pending_created_.clear();

    for (const auto& [num, _] : registry_->getVersionsMap()) {
        if (auto* cv = registry_->getVersion(num)) {
            if (cv->isDirty()) { cv->restore(); cv->clearDirty(); }
        }
    }
    pending_deleted_.clear();
    search_filter_.clear();
    search_buf_[0] = '\0';
    populateVersionData();
    if (active_version_ != 0) {
        if (auto* cv = registry_->getVersion(active_version_))
            syncFromClient(*cv);
    } else if (!filtered_versions_.empty()) {
        selectClient(filtered_versions_[0]);
    }
}

bool ClientConfigurationController::hasDirty() const {
    if (!registry_) return false;
    if (!pending_deleted_.empty()) return true;
    if (!pending_created_.empty()) return true;
    for (const auto& [num, _] : registry_->getVersionsMap())
        if (auto* cv = registry_->getVersion(num))
            if (cv->isDirty()) return true;
    return false;
}

bool ClientConfigurationController::validateBeforeSave() {
    std::unordered_set<std::string> names;
    for (const auto& [index, cv] : registry_->getVersionsMap()) {
        if (pending_deleted_.count(index)) continue;
        if (cv.getName().empty()) {
            validation_error_ = std::format("Client with index {} has an empty name.", index);
            return false;
        }
        if (!names.insert(cv.getName()).second) {
            validation_error_ = std::format("Duplicate client name: '{}'.", cv.getName());
            return false;
        }
    }
    return true;
}

// === Asset detection ===

void ClientConfigurationController::runAssetDetection() {
    if (active_version_ == 0 || !registry_) return;
    auto* cv = registry_->getVersion(active_version_);
    if (!cv) return;
    syncToClient(*cv);

    auto result = Services::ClientAssetDetector::detect(
        cv->getClientPath(), cv->getMetadataFile(), cv->getSpritesFile(),
        &registry_->getVersionsMap());

    applyDetectionResult(result);
    syncToClient(*cv);
    for (const auto& w : result.warnings)
        spdlog::warn("Asset detection: {}", w);
}

// === Filter / grouping ===

bool ClientConfigurationController::matchesFilter(const Domain::ClientVersion& ver) const {
    if (search_filter_.empty()) return true;
    std::string haystack = std::to_string(ver.getIndex()) + " " +
        toLower(ver.getName()) + " " + std::to_string(ver.getVersion()) + " " +
        toLower(ver.getDescription()) + " " +
        toLower(dataSourceToString(ver.getDataSource()));
    return haystack.find(search_filter_) != std::string::npos;
}

void ClientConfigurationController::populateVersionData() {
    filtered_versions_.clear();
    for (const auto& [index, cv] : registry_->getVersionsMap()) {
        if (pending_deleted_.count(index)) continue;
        if (!matchesFilter(cv)) continue;
        filtered_versions_.push_back(index);
    }
    std::sort(filtered_versions_.begin(), filtered_versions_.end());
}

void ClientConfigurationController::setSearchFilter(const std::string& filter) {
    search_filter_ = filter;
    populateVersionData();
}

// === Property state tracking ===

void ClientConfigurationController::setPropertyState(uint32_t version, const char* name,
                                                     Domain::PropertyVisualState state) {
    property_states_[version][name] = state;
}

Domain::PropertyVisualState
ClientConfigurationController::getPropertyState(uint32_t version, const char* name) const {
    auto it = property_states_.find(version);
    if (it == property_states_.end()) return Domain::PropertyVisualState::Default;
    auto pit = it->second.find(name);
    if (pit == it->second.end()) return Domain::PropertyVisualState::Default;
    return pit->second;
}

void ClientConfigurationController::clearPropertyStates(uint32_t version) {
    property_states_.erase(version);
}

void ClientConfigurationController::markAllPendingAsSaved(uint32_t version) {
    auto it = property_states_.find(version);
    if (it == property_states_.end()) return;
    for (auto& [name, state] : it->second)
        if (state == Domain::PropertyVisualState::Pending) state = Domain::PropertyVisualState::Saved;
}

const std::unordered_map<std::string, Domain::PropertyVisualState>&
ClientConfigurationController::getStates(uint32_t version) const {
    static const std::unordered_map<std::string, Domain::PropertyVisualState> kEmpty;
    auto it = property_states_.find(version);
    return it != property_states_.end() ? it->second : kEmpty;
}

// === Sync ===

void ClientConfigurationController::syncFromClient(const Domain::ClientVersion& cv) {
    version_int_ = static_cast<int>(cv.getVersion());
    auto cpy = [](char* dst, size_t sz, const std::string& src) {
        std::strncpy(dst, src.c_str(), sz - 1); dst[sz - 1] = '\0';
    };
    cpy(name_buf_, sizeof(name_buf_), cv.getName());
    cpy(desc_buf_, sizeof(desc_buf_), cv.getDescription());
    cpy(data_dir_buf_, sizeof(data_dir_buf_), cv.getDataDirectory());
    cpy(client_path_buf_, sizeof(client_path_buf_), cv.getClientPath().string());
    cpy(metadata_buf_, sizeof(metadata_buf_), cv.getMetadataFile());
    cpy(sprites_buf_, sizeof(sprites_buf_), cv.getSpritesFile());
    cpy(items_db_buf_, sizeof(items_db_buf_), cv.getCustomItemsDbPath().string());

    std::snprintf(dat_sig_buf_, sizeof(dat_sig_buf_), "%08X", cv.getDatSignature());
    std::snprintf(spr_sig_buf_, sizeof(spr_sig_buf_), "%08X", cv.getSprSignature());

    data_source_idx_ = static_cast<int>(cv.getDataSource());
    otb_id_int_ = static_cast<int>(cv.getOtbVersion());
    otb_major_int_ = static_cast<int>(cv.getOtbMajor());

    std::string otbm;
    for (size_t i = 0; i < cv.getMapVersionsSupported().size(); ++i) {
        if (i > 0) otbm += ", ";
        otbm += std::to_string(cv.getMapVersionsSupported()[i]);
    }
    cpy(otbm_versions_buf_, sizeof(otbm_versions_buf_), otbm);

    transparent_bool_ = cv.isTransparent();
    extended_bool_ = cv.isExtended();
    frame_durations_bool_ = cv.hasFrameDurations();
    frame_groups_bool_ = cv.hasFrameGroups();
    is_default_bool_ = cv.isDefault();
}

void ClientConfigurationController::syncToClient(Domain::ClientVersion& cv) {
    uint32_t new_version = static_cast<uint32_t>(std::max(0, version_int_));
    cv.setName(name_buf_);
    cv.setDescription(desc_buf_);
    cv.setDataDirectory(data_dir_buf_);
    cv.setClientPath(client_path_buf_);
    cv.setMetadataFile(metadata_buf_);
    cv.setSpritesFile(sprites_buf_);
    cv.setOtbMajor(static_cast<uint32_t>(std::max(0, otb_major_int_)));
    cv.setOtbVersion(static_cast<uint32_t>(std::max(0, otb_id_int_)));

    std::vector<uint32_t> otbm;
    std::istringstream iss(otbm_versions_buf_);
    std::string tok;
    while (std::getline(iss, tok, ',')) {
        try {
            uint32_t v = static_cast<uint32_t>(std::stoul(tok));
            if (v >= Domain::kOtbmVersionMin && v <= Domain::kOtbmVersionMax) otbm.push_back(v);
        } catch (...) {}
    }
    cv.setMapVersionsSupported(otbm);
    cv.setTransparent(transparent_bool_);
    cv.setExtended(extended_bool_);
    cv.setFrameDurations(frame_durations_bool_);
    cv.setFrameGroups(frame_groups_bool_);
    if (is_default_bool_ && registry_) registry_->setDefaultVersion(cv.getIndex());

    uint32_t ds = 0, ss = 0;
    std::istringstream(dat_sig_buf_) >> std::hex >> ds;
    std::istringstream(spr_sig_buf_) >> std::hex >> ss;
    cv.setDatSignature(ds);
    cv.setSprSignature(ss);
    if (cv.getVersion() != new_version) cv.setVersion(new_version);
    cv.setDataSource(static_cast<Domain::ItemDataSource>(
        std::clamp(data_source_idx_, 0, static_cast<int>(Domain::ItemDataSource::DAT))));
    cv.setCustomItemsDbPath(items_db_buf_[0] != '\0' ? std::filesystem::path(items_db_buf_)
                                                      : std::filesystem::path());
    cv.markDirty();
}

void ClientConfigurationController::applyDetectionResult(
    const Domain::ClientAssetDetectionResult& result) {
    auto& states = property_states_[active_version_];
    auto apply = [&](const char* name, bool detected) {
        states[name] = detected ? Domain::PropertyVisualState::Pending
                                : Domain::PropertyVisualState::Undetected;
    };

    auto cpy = [](char* dst, size_t sz, const std::string& src) {
        std::strncpy(dst, src.c_str(), sz - 1); dst[sz - 1] = '\0';
    };

    if (result.metadata_file_name)
        cpy(metadata_buf_, sizeof(metadata_buf_), *result.metadata_file_name);
    apply("metadataFile", result.metadata_file_name.has_value());

    if (result.sprites_file_name)
        cpy(sprites_buf_, sizeof(sprites_buf_), *result.sprites_file_name);
    apply("spritesFile", result.sprites_file_name.has_value());

    if (result.dat_signature)
        std::snprintf(dat_sig_buf_, sizeof(dat_sig_buf_), "%08X", *result.dat_signature);
    apply("datSignature", result.dat_signature.has_value());

    if (result.spr_signature)
        std::snprintf(spr_sig_buf_, sizeof(spr_sig_buf_), "%08X", *result.spr_signature);
    apply("sprSignature", result.spr_signature.has_value());

    if (result.transparency) transparent_bool_ = *result.transparency;
    apply("transparency", result.transparency.has_value());
    if (result.extended) extended_bool_ = *result.extended;
    apply("extended", result.extended.has_value());
    if (result.frame_durations) frame_durations_bool_ = *result.frame_durations;
    apply("frameDurations", result.frame_durations.has_value());
    if (result.frame_groups) frame_groups_bool_ = *result.frame_groups;
    apply("frameGroups", result.frame_groups.has_value());
}

// === Path dependency ===

void ClientConfigurationController::autoDetectFromClientPath(
    const std::filesystem::path& clientPath) {
    auto_filling_ = true;
    auto cpy = [](char* dst, size_t sz, const std::string& src) {
        std::strncpy(dst, src.c_str(), sz - 1); dst[sz - 1] = '\0';
    };
    cpy(metadata_buf_, sizeof(metadata_buf_), (clientPath / "Tibia.dat").string());
    cpy(sprites_buf_, sizeof(sprites_buf_), (clientPath / "Tibia.spr").string());

    if (auto* cv = registry_->getVersion(active_version_)) {
        cv->setMetadataFile((clientPath / "Tibia.dat").string());
        cv->setSpritesFile((clientPath / "Tibia.spr").string());
        for (const auto& name : {"items.otb", "items.srv"}) {
            auto test_path = clientPath / name;
            if (std::filesystem::exists(test_path)) {
                auto path_str = test_path.string();
                cpy(items_db_buf_, sizeof(items_db_buf_), path_str);
                cv->setCustomItemsDbPath(test_path);
                break;
            }
        }
    }
    auto_filling_ = false;
}

void ClientConfigurationController::invalidateClientPath() {
    client_path_buf_[0] = '\0';
    if (auto* cv = registry_->getVersion(active_version_)) cv->setClientPath("");
    property_states_[active_version_]["clientPath"] = Domain::PropertyVisualState::Pending;
}

// === Render context ===

ClientConfigurationController::Buffers ClientConfigurationController::getBuffers() const {
    return {name_buf_, desc_buf_, data_dir_buf_, client_path_buf_, metadata_buf_,
            sprites_buf_, items_db_buf_, dat_sig_buf_, spr_sig_buf_, otbm_versions_buf_,
            data_source_idx_, otb_id_int_, otb_major_int_,
            transparent_bool_, extended_bool_, frame_durations_bool_,
            frame_groups_bool_, is_default_bool_, version_int_};
}

ClientConfigurationController::EditableBuffers ClientConfigurationController::getEditableBuffers() {
    return {name_buf_, sizeof(name_buf_), desc_buf_, sizeof(desc_buf_),
            data_dir_buf_, sizeof(data_dir_buf_), client_path_buf_, sizeof(client_path_buf_),
            metadata_buf_, sizeof(metadata_buf_), sprites_buf_, sizeof(sprites_buf_),
            items_db_buf_, sizeof(items_db_buf_), dat_sig_buf_, sizeof(dat_sig_buf_),
            spr_sig_buf_, sizeof(spr_sig_buf_), otbm_versions_buf_, sizeof(otbm_versions_buf_),
            &data_source_idx_, &otb_id_int_, &otb_major_int_,
            &transparent_bool_, &extended_bool_, &frame_durations_bool_,
            &frame_groups_bool_, &is_default_bool_, &version_int_};
}

} // namespace Presentation
} // namespace MapEditor
