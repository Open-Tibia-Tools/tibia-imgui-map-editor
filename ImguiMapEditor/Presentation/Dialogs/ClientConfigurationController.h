#pragma once

#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include "Services/ClientVersionRegistry.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MapEditor {
namespace Services {
class ConfigService;
class ClientAssetDetector;
}

namespace Presentation {

class ClientConfigurationController {
public:
    using ChangeCallback = std::function<void()>;

    void open(Services::ClientVersionRegistry& registry, Services::ConfigService& config);
    void close();
    bool isOpen() const { return is_open_; }

    Services::ClientVersionRegistry* registry() const { return registry_; }

    void setChangeCallback(ChangeCallback cb) { on_change_ = std::move(cb); }

    // === Selection ===
    uint32_t activeVersion() const { return active_version_; }
    void selectClient(uint32_t version);

    // === CRUD ===
    void addClient();
    void addClientWithVersion(const std::string& name, uint32_t version);
    void duplicateClient();
    void deleteClient(uint32_t version);
    void duplicateClient(uint32_t from_version);

    // === Persistence ===
    bool saveAll();
    void discardChanges();

    // === Validation ===
    const std::string& validationError() const { return validation_error_; }

    // === Dirty state ===
    bool hasDirty() const;

    // === Asset detection ===
    void runAssetDetection();

    // === Filter / grouping ===
    struct VersionGroup {
        int major;
        std::string label;
        bool visible = true;
        std::vector<uint32_t> versions;
    };
    const std::vector<VersionGroup>& versionGroups() const { return version_groups_; }
    const std::vector<uint32_t>& filteredVersions() const { return filtered_versions_; }

    void populateVersionData();
    void setSearchFilter(const std::string& filter);

    // === Property state tracking ===
    void setPropertyState(uint32_t version, const char* name, Domain::PropertyVisualState state);
    Domain::PropertyVisualState getPropertyState(uint32_t version, const char* name) const;
    void clearPropertyStates(uint32_t version);
    void markAllPendingAsSaved(uint32_t version);
    const std::unordered_map<std::string, Domain::PropertyVisualState>&
    getStates(uint32_t version) const;

    // === Sync (buffer ↔ domain) ===
    void syncFromClient(const Domain::ClientVersion& cv);
    void syncToClient(Domain::ClientVersion& cv);
    void applyDetectionResult(const Domain::ClientAssetDetectionResult& result);

    // === Path dependency ===
    bool isAutoFilling() const { return auto_filling_; }
    void autoDetectFromClientPath(const std::filesystem::path& clientPath);
    void invalidateClientPath();

    // === Render context (buffer access for views) ===
    struct Buffers {
        const char* name; const char* desc; const char* dataDir;
        const char* clientPath; const char* metadata; const char* sprites;
        const char* itemsDb; const char* datSig; const char* sprSig;
        const char* otbmVersions;
        int dataSourceIdx; int otbId; int otbMajor;
        bool transparent; bool extended; bool frameDurations;
        bool frameGroups; bool isDefault;
        int versionInt;
    };
    Buffers getBuffers() const;

    struct EditableBuffers {
        char* name; int nameSize;
        char* desc; int descSize;
        char* dataDir; int dataDirSize;
        char* clientPath; int clientPathSize;
        char* metadata; int metadataSize;
        char* sprites; int spritesSize;
        char* itemsDb; int itemsDbSize;
        char* datSig; int datSigSize;
        char* sprSig; int sprSigSize;
        char* otbmVersions; int otbmVersionsSize;
        int* dataSourceIdx;
        int* otbId; int* otbMajor;
        bool* transparent; bool* extended; bool* frameDurations;
        bool* frameGroups; bool* isDefault;
        int* versionInt;
    };
    EditableBuffers getEditableBuffers();

private:
    static std::string buildGroupLabel(int major);
    int getMajorGroup(uint32_t version) const;
    bool matchesFilter(const Domain::ClientVersion& ver) const;
    bool validateBeforeSave();

    Services::ClientVersionRegistry* registry_ = nullptr;
    Services::ConfigService* config_ = nullptr;
    bool is_open_ = false;

    uint32_t active_version_ = 0;
    int active_tab_ = 0;
    std::string search_filter_;
    char search_buf_[128] = {};

    std::vector<VersionGroup> version_groups_;
    std::vector<uint32_t> filtered_versions_;
    std::unordered_set<uint32_t> pending_deleted_;
    std::unordered_set<uint32_t> pending_created_;
    std::string validation_error_;

    ChangeCallback on_change_;

    // === Editor buffers ===
    char name_buf_[64] = {};
    char desc_buf_[256] = {};
    char data_dir_buf_[512] = {};
    char client_path_buf_[512] = {};
    char metadata_buf_[512] = {};
    char sprites_buf_[512] = {};
    char items_db_buf_[512] = {};
    char dat_sig_buf_[16] = {};
    char spr_sig_buf_[16] = {};
    char otbm_versions_buf_[64] = {};
    int version_int_ = 0;
    int data_source_idx_ = 0;
    int otb_id_int_ = 0;
    int otb_major_int_ = 0;
    bool transparent_bool_ = false;
    bool extended_bool_ = false;
    bool frame_durations_bool_ = false;
    bool frame_groups_bool_ = false;
    bool is_default_bool_ = false;
    bool auto_filling_ = false;

    std::unordered_map<uint32_t, std::unordered_map<std::string, Domain::PropertyVisualState>>
        property_states_;
};

} // namespace Presentation
} // namespace MapEditor
