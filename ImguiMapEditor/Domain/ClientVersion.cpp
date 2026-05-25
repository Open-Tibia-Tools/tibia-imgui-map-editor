#include "ClientVersion.h"
namespace MapEditor {
namespace Domain {

ClientVersion::ClientVersion(uint32_t version, const std::string& name, uint32_t otb_version)
    : version_(version)
    , name_(name)
    , otb_version_(otb_version)
{
}

std::filesystem::path ClientVersion::getDatPath() const {
    if (metadata_file_.empty()) return {};
    std::filesystem::path p(metadata_file_);
    if (p.is_absolute()) return p;
    if (client_path_.empty()) return {};
    return client_path_ / p;
}

std::filesystem::path ClientVersion::getSprPath() const {
    if (sprites_file_.empty()) return {};
    std::filesystem::path p(sprites_file_);
    if (p.is_absolute()) return p;
    if (client_path_.empty()) return {};
    return client_path_ / p;
}

std::filesystem::path ClientVersion::getItemMetadataPath() const {
    if (!custom_items_db_path_.empty()) {
        return custom_items_db_path_;
    }
    if (client_path_.empty()) {
        return {};
    }
    switch (data_source_) {
    case ItemDataSource::SRV:
        return client_path_ / "items.srv";
    case ItemDataSource::OTB:
        return client_path_ / "items.otb";
    case ItemDataSource::DAT:
    default:
        return {};
    }
}

bool ClientVersion::hasValidPaths() const {
    if (client_path_.empty()) {
        return false;
    }
    return std::filesystem::exists(client_path_);
}

bool ClientVersion::validateFiles() const {
    if (!hasValidPaths()) {
        return false;
    }
    
    auto dat_path = getDatPath();
    auto spr_path = getSprPath();
    
    if (!std::filesystem::exists(dat_path) || !std::filesystem::exists(spr_path)) {
        return false;
    }

    switch (data_source_) {
    case ItemDataSource::OTB:
    case ItemDataSource::SRV: {
        auto path = getItemMetadataPath();
        if (std::filesystem::exists(path)) {
            return true;
        }
        // Fallback check in data/ directory
        auto filename = (data_source_ == ItemDataSource::SRV) ? "items.srv" : "items.otb";
        return std::filesystem::exists(std::filesystem::current_path() / "data" / filename);
    }
    case ItemDataSource::DAT:
        return true;
    default:
        break;
    }

    return false;
}

void ClientVersion::backup() {
    backup_data_ = BackupData{
        version_,
        name_,
        otb_version_,
        otb_major_,
        map_versions_supported_,
        dat_signature_,
        spr_signature_,
        client_path_,
        data_directory_,
        description_,
        metadata_file_,
        sprites_file_,
        data_source_,
        visible_,
        is_default_,
        transparency_,
        extended_,
        frame_durations_,
        frame_groups_,
        dat_format_,
        custom_items_db_path_,
    };
}

void ClientVersion::restore() {
    version_ = backup_data_.version;
    name_ = backup_data_.name;
    otb_version_ = backup_data_.otb_version;
    otb_major_ = backup_data_.otb_major;
    map_versions_supported_ = backup_data_.map_versions_supported;
    dat_signature_ = backup_data_.dat_signature;
    spr_signature_ = backup_data_.spr_signature;
    client_path_ = backup_data_.client_path;
    data_directory_ = backup_data_.data_directory;
    description_ = backup_data_.description;
    metadata_file_ = backup_data_.metadata_file;
    sprites_file_ = backup_data_.sprites_file;
    data_source_ = backup_data_.data_source;
    visible_ = backup_data_.visible;
    is_default_ = backup_data_.is_default;
    transparency_ = backup_data_.transparency;
    extended_ = backup_data_.extended;
    frame_durations_ = backup_data_.frame_durations;
    frame_groups_ = backup_data_.frame_groups;
    dat_format_ = backup_data_.dat_format;
    custom_items_db_path_ = backup_data_.custom_items_db_path;
}

} // namespace Domain
} // namespace MapEditor
