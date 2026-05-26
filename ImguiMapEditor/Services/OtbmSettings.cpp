#include "OtbmSettings.h"
#include "ConfigService.h"

namespace MapEditor {
namespace Services {

void OtbmSettings::loadFromConfig(const ConfigService& config) {
    int read_val = config.get<int>("otbm.waypoint_read", static_cast<int>(Domain::OtbmReadSource::Otbm));
    if (read_val < 0 || read_val > static_cast<int>(Domain::OtbmReadSource::Xml)) {
        read_val = static_cast<int>(Domain::OtbmReadSource::Otbm);
    }
    waypoint_read = static_cast<Domain::OtbmReadSource>(read_val);

    int write_val = config.get<int>("otbm.waypoint_write", static_cast<int>(Domain::OtbmWriteTarget::Otbm));
    if (write_val < 0 || write_val > static_cast<int>(Domain::OtbmWriteTarget::Xml)) {
        write_val = static_cast<int>(Domain::OtbmWriteTarget::Otbm);
    }
    waypoint_write = static_cast<Domain::OtbmWriteTarget>(write_val);
}

void OtbmSettings::saveToConfig(ConfigService& config) const {
    config.set("otbm.waypoint_read", static_cast<int>(waypoint_read));
    config.set("otbm.waypoint_write", static_cast<int>(waypoint_write));
}

} // namespace Services
} // namespace MapEditor
