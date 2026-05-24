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

namespace UI {

class ClientPropertyEditor;

class ClientConfigurationDialog {
public:
  ClientConfigurationDialog();
  ~ClientConfigurationDialog();

  void open(Services::ClientVersionRegistry &registry,
            Services::ConfigService &config);
  bool render();
  void close();
  bool isOpen() const { return is_open_; }

private:
  static int getMajorGroup(uint32_t version) ;
  bool matchesFilter(const Domain::ClientVersion &version) const;
  void populateVersionData();
  void selectClient(uint32_t version);
  void runAssetDetection();

  void renderTitleBar();
  void renderToolbar();
  void renderBody();
  void renderLeftSidebar();
  void renderVersionList();
  void renderRightPanel();
  void renderFooterStatus();

  void addClient();
  void duplicateClient();
  void deleteClient(uint32_t version);
  bool saveAll();
  void discardChanges();
  bool validateBeforeSave();

  Services::ClientVersionRegistry *registry_ = nullptr;
  Services::ConfigService *config_ = nullptr;
  bool is_open_ = false;

  uint32_t active_version_ = 0;
  int active_tab_ = 0;
  char search_buf_[128] = {};
  std::string search_filter_;

  struct VersionGroup {
    int major;
    std::string label;
    std::vector<uint32_t> versions;
  };
  std::vector<VersionGroup> version_groups_;

  std::vector<uint32_t> filtered_versions_;

  std::unordered_set<uint32_t> pending_deleted_;
  std::string validation_error_;

  std::unique_ptr<ClientPropertyEditor> editor_;
};

} // namespace UI
} // namespace MapEditor
