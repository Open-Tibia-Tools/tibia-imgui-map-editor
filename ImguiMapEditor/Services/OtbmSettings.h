#pragma once
#include "Domain/OtbmDataTypes.h"
#include <cstdint>

namespace MapEditor {
namespace Services {

class ConfigService;

struct OtbmSettings {
    Domain::OtbmReadSource waypoint_read = Domain::OtbmReadSource::Otbm;
    Domain::OtbmWriteTarget waypoint_write = Domain::OtbmWriteTarget::Otbm;

    // Placeholders for future data types
    // Domain::OtbmReadSource spawn_read = Domain::OtbmReadSource::Otbm;
    // Domain::OtbmWriteTarget spawn_write = Domain::OtbmWriteTarget::Otbm;
    // Domain::OtbmReadSource house_read = Domain::OtbmReadSource::Otbm;
    // Domain::OtbmWriteTarget house_write = Domain::OtbmWriteTarget::Otbm;
    // Domain::OtbmReadSource town_read = Domain::OtbmReadSource::Otbm;
    // Domain::OtbmWriteTarget town_write = Domain::OtbmWriteTarget::Otbm;
    // Domain::OtbmReadSource zone_read = Domain::OtbmReadSource::Otbm;
    // Domain::OtbmWriteTarget zone_write = Domain::OtbmWriteTarget::Otbm;

    void loadFromConfig(const ConfigService& config);
    void saveToConfig(ConfigService& config) const;
};

} // namespace Services
} // namespace MapEditor
