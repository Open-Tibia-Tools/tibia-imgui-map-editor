#pragma once

#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace MapEditor {
namespace Services { class ClientVersionRegistry; }
namespace Presentation { class ClientConfigurationController; }

namespace UI {

class ClientPropertyEditor {
public:
    void setRegistry(Services::ClientVersionRegistry* registry) { registry_ = registry; }
    void setActiveVersion(uint32_t version) { active_version_ = version; }
    void setController(Presentation::ClientConfigurationController* ctrl) { controller_ = ctrl; }

    void render();
    void renderStatusBar();

private:
    void renderIdentitySection();
    void renderFilesSection();
    void renderClientPathField();
    void renderDataDirectoryField();
    void renderMetadataFileField();
    void renderSpritesFileField();
    void renderItemsDatabaseField();
    void renderCompatibilitySection();
    void renderSignaturesSection();
    void renderFeaturesSection();

    Services::ClientVersionRegistry* registry_ = nullptr;
    Presentation::ClientConfigurationController* controller_ = nullptr;
    uint32_t active_version_ = 0;
};

} // namespace UI
} // namespace MapEditor
