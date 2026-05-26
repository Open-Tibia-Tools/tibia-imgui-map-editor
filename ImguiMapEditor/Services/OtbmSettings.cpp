#include "OtbmSettings.h"
#include "ConfigService.h"

namespace MapEditor {
namespace Services {

void OtbmSettings::loadFromConfig(const ConfigService& config) {
    waypoint_read = static_cast<Domain::OtbmReadSource>(
        config.get<int>("otbm.waypoint_read", static_cast<int>(Domain::OtbmReadSource::Otbm)));
    waypoint_write = static_cast<Domain::OtbmWriteTarget>(
        config.get<int>("otbm.waypoint_write", static_cast<int>(Domain::OtbmWriteTarget::Otbm)));
}

void OtbmSettings::saveToConfig(ConfigService& config) const {
    config.set("otbm.waypoint_read", static_cast<int>(waypoint_read));
    config.set("otbm.waypoint_write", static_cast<int>(waypoint_write));
}

} // namespace Services
} // namespace MapEditor
