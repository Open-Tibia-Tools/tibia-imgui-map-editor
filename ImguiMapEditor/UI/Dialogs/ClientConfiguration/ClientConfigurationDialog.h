#pragma once

#include "Domain/ClientVersion.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace MapEditor {
namespace Services {
class ClientVersionRegistry;
class ConfigService;
} // namespace Services

namespace Presentation {
class ClientConfigurationController;
} // namespace Presentation

namespace UI {

class ClientPropertyEditor;

class ClientConfigurationDialog {
public:
    ClientConfigurationDialog();
    ~ClientConfigurationDialog();

    void open(Presentation::ClientConfigurationController& controller,
              Services::ClientVersionRegistry& registry,
              Services::ConfigService& config);
    bool render();
    void close();
    bool isOpen() const { return is_open_; }

private:
    void renderTitleBar();
    void renderToolbar();
    void renderBody();
    void renderLeftSidebar();
    void renderVersionList();
    void renderRightPanel();
    void renderFooterStatus();

    bool is_open_ = false;

    Presentation::ClientConfigurationController* controller_ = nullptr;
    std::unique_ptr<ClientPropertyEditor> editor_;
    char search_buf_[128] = {};
};

} // namespace UI
} // namespace MapEditor
