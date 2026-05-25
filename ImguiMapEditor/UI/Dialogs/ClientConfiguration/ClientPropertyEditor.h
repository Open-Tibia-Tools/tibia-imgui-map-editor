#pragma once

#include "Domain/ClientVersion.h"
#include "Domain/ClientVersionTypes.h"
#include "Services/ClientVersionRegistry.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace MapEditor {
namespace UI {

class ClientPropertyEditor {
public:
  using ChangeCallback = std::function<void()>;

  void setRegistry(Services::ClientVersionRegistry *registry) {
    registry_ = registry;
  }
  void setActiveVersion(uint32_t version) { active_version_ = version; }
  void setChangeCallback(ChangeCallback cb) { on_change_ = std::move(cb); }

  void setPropertyState(uint32_t version, const char *name,
                        Domain::PropertyVisualState state);
  Domain::PropertyVisualState getPropertyState(uint32_t version,
                                               const char *name) const;
  void clearPropertyStates(uint32_t version);
  void markAllPendingAsSaved(uint32_t version);

  void syncFromClient(const Domain::ClientVersion &cv);
  void syncToClient(Domain::ClientVersion &cv);
  void applyDetectionResult(const Domain::ClientAssetDetectionResult &result);

  void render();
  void renderStatusBar();

private:
  void renderIdentitySection();
  void renderFilesSection();
  void renderCompatibilitySection();
  void renderSignaturesSection();
  void renderFeaturesSection();

  void beginSection(const char *title);
  void endSection();

  Services::ClientVersionRegistry *registry_ = nullptr;
  uint32_t active_version_ = 0;
  ChangeCallback on_change_;

  std::unordered_map<uint32_t,
                     std::unordered_map<std::string, Domain::PropertyVisualState>>
      property_states_;

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
};

} // namespace UI
} // namespace MapEditor
