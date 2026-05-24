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
    if (client_path_.empty()) {
        return {};
    }
    return client_path_ / (metadata_file_.empty() ? "Tibia.dat" : metadata_file_);
}

std::filesystem::path ClientVersion::getSprPath() const {
    if (client_path_.empty()) {
        return {};
    }
    return client_path_ / (sprites_file_.empty() ? "Tibia.spr" : sprites_file_);
}

std::filesystem::path ClientVersion::getOtbPath() const {
    if (client_path_.empty()) {
        return {};
    }
    if (data_source_ == ItemDataSource::SRV) {
        return client_path_ / "items.srv";
    }
    return client_path_ / "items.otb";
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

    if (data_source_ == ItemDataSource::OTB) {
        return std::filesystem::exists(getOtbPath());
    } else if (data_source_ == ItemDataSource::SRV) {
        return std::filesystem::exists(getOtbPath()); // returns items.srv path
    } else if (data_source_ == ItemDataSource::DAT) {
        return true; // only dat + spr required
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
}

} // namespace Domain
} // namespace MapEditor
