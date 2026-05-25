# Client Configuration Rewrite Plan

## Summary
Replace the 4 broken UI components with a single split-panel window. Extend data model with feature flags, dirty tracking, and a new detection engine leveraging existing DAT/SPR readers.

## Files to Create
1. `Domain/ClientVersionTypes.h` — shared types
2. `Services/ClientAssetDetector.h` + `.cpp` — detection engine

## Files to Modify
3. `Domain/ClientVersion.h` + `.cpp` — extend model
4. `Services/ClientVersionPersistence.h` + `.cpp` — extend JSON I/O
5. `Services/ClientVersionRegistry.h` + `.cpp` — add backup/restore
6. `Services/ClientSignatureDetector.h` + `.cpp` — thin wrapper over new detector
7. `UI/Dialogs/ClientConfiguration/ClientConfigurationDialog.h` + `.cpp` — total rewrite
8. `clients.json` — add new fields
9. `Controllers/StartupController.cpp` — update include
10. `Services/ClientVersionValidator.cpp` — update include
11. `Services/SecondaryClientData.cpp` — update include
12. `UI/Dialogs/Startup/StartupDialog.cpp` — remove dead references

## Files to Delete
13. `UI/Dialogs/ClientConfiguration/ClientEditModal.h` + `.cpp`
14. `UI/Dialogs/ClientConfiguration/ClientTableWidget.h` + `.cpp`
15. `UI/Dialogs/ClientConfiguration/ClientDetailsCard.h` + `.cpp`
16. `UI/DTOs/ClientEditData.h`

---

## Phase 1: Create `Domain/ClientVersionTypes.h`
- `enum class DatFormat : uint8_t` — 9 values (Unknown, Format74 through Format1057)
- `struct ClientAssetDetectionResult` — optional fields for all detected properties + warnings vector
- `enum class PropertyVisualState` — Default, Pending, Undetected, Saved

## Phase 2: Extend `Domain/ClientVersion.h`
Add members:
```cpp
// Feature flags
bool transparency_ = false;
bool extended_ = false;
bool frame_durations_ = false;
bool frame_groups_ = false;

// Configurable file names (not hardcoded "Tibia.dat"/"Tibia.spr")
std::string metadata_file_ = "Tibia.dat";
std::string sprites_file_ = "Tibia.spr";

// Full OTBM versions array (fixes truncation bug)
std::vector<uint32_t> map_versions_supported_;

// Dirty tracking
bool is_dirty_ = false;
struct BackupData { /* all fields */ } backup_data_;
```
Add getters/setters for all new fields. Update `getDatPath()`, `getSprPath()` to use `metadata_file_`/`sprites_file_`. Add `backup()`, `restore()`, `markDirty()`, `clearDirty()`, `isDirty()`.

Remove `otbm_version_` single field — use `map_versions_supported_` instead. Keep `getOtbmVersion()` returning first element for compatibility.

## Phase 3: Extend `ClientVersionPersistence`
**loadFromJson** — add reading of:
- `"transparency"`, `"extended"`, `"frameDurations"`, `"frameGroups"` → bool with default false
- `"metadataFile"` → string with default "Tibia.dat"
- `"spritesFile"` → string with default "Tibia.spr"
- `"otbmVersions"` → read ALL array elements into `map_versions_supported_`

**saveToJson** — add writing of:
- All four feature flags (bool)
- `metadataFile`, `spritesFile`
- Full `otbmVersions` array

## Phase 4: Extend `ClientVersionRegistry`
Add methods:
```cpp
void backupVersion(uint32_t version_number);
void restoreVersion(uint32_t version_number);
```
In `updateClient()` — preserve backup data from old version.

## Phase 5: Create `ClientAssetDetector`
Leverages IMGUI's existing `BinaryReader`, `DatReaderFactory`, `SprReader`:

**Public API:**
```cpp
class ClientAssetDetector {
public:
    // Full detection (used by config dialog when user picks a folder)
    static Domain::ClientAssetDetectionResult detect(
        const std::filesystem::path& client_path,
        const std::string& metadata_file,
        const std::string& sprites_file);

    // Simple version lookup (backward compat with old ClientSignatureDetector API)
    static uint32_t detectVersion(
        const std::filesystem::path& folder,
        const std::map<uint32_t, Domain::ClientVersion>& versions);
};
```

**detect() implementation:**
1. Read DAT signature: BinaryReader reads 4-byte header from metadata_file
2. Read SPR signature: BinaryReader reads 4-byte header from sprites_file
3. **SPR structure probing** (ported from RME):
   - After signature, read sprite count (try both U16 and U32 to detect extended)
   - Collect up to 24 sample sprite offsets, validate RLE encoding with both 3-byte (opaque) and 4-byte (alpha) pixel formats
   - Return `transparency`, `extended`, `spr_signature`
4. Return `ClientAssetDetectionResult` with all detected values

**detectVersion() implementation:**
- Calls `detect()` for signature reading
- Matches DAT+SPR signatures against known versions
- Falls back to DAT-only match

**detect() DOES NOT do DAT parsing** — it only does header/signature reading + SPR structure probing. DAT parsing is too heavy for detection (would require loading the full client). Feature detection beyond extended/transparency is inferred from version number once the version is matched.

## Phase 6: Update `ClientSignatureDetector`
Make it a thin wrapper:
```cpp
static uint32_t detectFromFolder(const std::filesystem::path& folder,
    const std::map<uint32_t, Domain::ClientVersion>& versions) {
    return ClientAssetDetector::detectVersion(folder, versions);
}
// readDatSignature / readSprSignature removed (internal to ClientAssetDetector)
```
Keep the class for backward compatibility. All 3 callers continue working.

## Phase 7: Rewrite `ClientConfigurationDialog` — FULL REWRITE

### Layout Structure
```
+--------------------------------------------------------------+
| "Client Configuration"                                        |
|--------------------------------------------------------------|
| [Add] [Duplicate] [Delete]     Search: [.....]                |
|--------------------------------------------------------------|
| Left (280px)                   | Right (stretch)              |
| Grouped Tree                   | Scrollable Property Editor   |
|   7.7.x (3)  [─]              |                              |
|   8.8.x (23) [+]              | === Identity ===            |
|   9.9.x (12) [+]             | ...                          |
|   10.x  (8) [+]              | === Files & Paths ===        |
|   12.x  (5) [+]              | ...                          |
|                               | === Compatibility ===        |
|                               | ...                          |
|                               | === Signatures ===           |
|                               | ...                          |
|                               | === Features ===             |
|                               | ...                          |
|--------------------------------------------------------------|
| Default Client: [dropdown▼]  [✓] Check Signatures            |
|                                        [Save] [Discard] [Close]|
+--------------------------------------------------------------+
```

### Key Implementation Details

#### Left Pane: Grouped Tree
- Use `ImGui::TreeNodeEx` with `ImGuiTreeNodeFlags_DefaultOpen` for groups
- Groups: `GetMajorGroup()` — same algorithm as RME:
  ```
  if version >= 10000 → version / 1000
  else if version >= 100 → version / 100
  else → version / 10
  ```
  This creates: 7.x (740, 750, 755...), 8.x, 9.x, 10.x, 12.x, 13.x
- Each leaf uses `ImGui::Selectable` — click selects, double-click enters edit
- Search filters by: name, version string, description, OTB ID (case-insensitive)
- Right-click context menu on leaf: "Duplicate", "Delete"
- Add button: creates new client with name "New Client N", default data dir "1287"
- Duplicate button: clones selected + appends " (Copy)"
- Delete button: marks as pending deletion (hidden from tree, committed on Save)

#### Right Pane: Property Editor
- Each section uses `ImGui::TreeNodeEx` as collapsible header
- All sections default-open except maybe Features
- Fields use `ImGui::InputText`, `ImGui::InputInt`, `ImGui::Checkbox`
- **Browse button**:
  ```cpp
  NFD::UniquePath outPath;
  if (NFD::PickFolder(outPath) == NFD_OKAY) {
      edit_data_.client_path = outPath.get();
      // Trigger detection
      auto result = ClientAssetDetector::detect(...);
      applyDetectionResult(client, result);
  }
  ```
- **Auto-detection visual states**:
  - Before detection: no special border
  - After detection: colored border based on `PropertyVisualState`
    - Pending = yellow overlay (detected but not saved)
    - Undetected = red overlay (could not detect)
    - Saved = green overlay (confirmed)
  - Implementation: `ImGui::PushStyleColor(ImGuiCol_FrameBg, ...)` before the input widget
  - The 8 auto-detected properties: metadataFile, spritesFile, datSignature, sprSignature, transparency, extended, frameDurations, frameGroups

- **Summary line**: Shows `"8.60"` + status badge: "Modified" (yellow) or "Saved" (green)

#### Dirty Tracking
- On selecting a new client: `backup()` current client, load new
- On any field change: `markDirty()` client
- **Discard**: `restore()` all dirty versions, clear property states
- **Save**: validate → write JSON → `clearDirty()` all → mark property states as Saved

#### Save Flow
```cpp
void save() {
    // 1. Validate
    for (auto& [id, client] : registry->getVersionsMap()) {
        if (client.getName().empty()) { show error; return; }
        // check duplicate names
    }

    // 2. Commit deletions
    for (auto& id : pending_deleted_) {
        registry->removeClient(id);
    }

    // 3. Serialize & write
    ClientVersionsData data;
    data.versions = registry->getVersionsMap();
    data.otb_to_version = registry->getOtbMapping();
    data.default_version = registry->getDefaultVersion();
    if (!ClientVersionPersistence::saveToJson(path, data)) {
        // Rollback deletions
        show error; return;
    }

    // 4. Mark clean
    for (auto* client : all_clients) { client->clearDirty(); }
    markPropertiesAsSaved();
    pending_deleted_.clear();
}
```

#### Deletion UX
- Delete button → marks client in `pending_deleted_client_ids` set
- Deleted clients hidden from tree (filtered in `MatchesFilter()`)
- On Save: commits deletions
- On Discard: clears pending deletions

#### Context Menu
```cpp
if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Duplicate")) { duplicateClient(); }
    if (ImGui::MenuItem("Delete")) { deleteClient(); }
    ImGui::EndPopup();
}
```

### State Variables
```cpp
ClientVersionRegistry* registry_ = nullptr;
bool is_open_ = false;

// Selection
uint32_t active_version_ = 0;
char search_buffer_[128] = {};

// Tree
bool ignore_tree_selection_ = false;

// Dirty tracking
struct PropertyState { PropertyVisualState state; };
std::unordered_map<uint32_t, std::unordered_map<std::string, PropertyVisualState>> property_states_;
std::unordered_set<uint32_t> pending_deleted_client_ids_;

// Settings
uint32_t default_version_ = 0;
bool check_signatures_ = true;

// Copy of active client for edit fields (to avoid modifying registry directly on each keystroke)
// Instead: edit directly on registry_->getVersion(active_version_) and mark dirty
// On Discard: restore from backup

// Property editor field buffers (char arrays for InputText)
char name_buf_[64];
char desc_buf_[256];
char data_dir_buf_[64];
char client_path_buf_[512];
char metadata_buf_[64];
char sprites_buf_[64];
char dat_sig_buf_[16];
char spr_sig_buf_[16];
char otbm_versions_buf_[64]; // comma-separated
int version_int_;
int otb_id_int_;
int otb_major_int_;
bool transparency_bool_;
bool extended_bool_;
bool frame_durations_bool_;
bool frame_groups_bool_;
bool is_default_bool_;
```

### Sync Functions
```cpp
void syncModelToEditor(const ClientVersion& cv) {
    // Fill all editor buffers from ClientVersion
    strncpy(name_buf_, cv.getName().c_str(), 63);
    version_int_ = cv.getVersion();
    // ... all fields
}

void syncEditorToModel(ClientVersion& cv) {
    // Apply all editor buffers back to ClientVersion
    cv.setName(name_buf_);
    // ... all fields
    cv.markDirty();
}
```

### Property State Coloring
For each auto-detected property, before rendering the input widget:
```cpp
auto state = getPropertyState(active_version_, property_name);
if (state == Pending)    PushStyleColor(FrameBg, yellowTint);
if (state == Undetected) PushStyleColor(FrameBg, redTint);
if (state == Saved)      PushStyleColor(FrameBg, greenTint);
// render widget
if (state != Default) PopStyleColor();
```

Auto-detected properties are: metadataFile, spritesFile, datSignature, sprSignature, transparency, extended, frameDurations, frameGroups

### Apply Detection Result
```cpp
void applyDetectionResult(ClientVersion& cv, const ClientAssetDetectionResult& r) {
    auto setPending = [&](const char* name) {
        setPropertyState(active_version_, name, PropertyVisualState::Pending);
    };
    auto setUndetected = [&](const char* name) {
        setPropertyState(active_version_, name, PropertyVisualState::Undetected);
    };

    if (r.metadata_file_name) { cv.setMetadataFile(*r.metadata_file_name); setPending("metadataFile"); }
    else setUndetected("metadataFile");
    // ... same for all 8 properties

    for (auto& w : r.warnings) { spdlog::warn("{}", w); }

    syncModelToEditor(cv);
}
```

---

## Phase 8: Update `clients.json`
Add to each of the 67 entries:
```json
"transparency": false,
"extended": false,
"frameDurations": false,
"frameGroups": false,
"metadataFile": "Tibia.dat",
"spritesFile": "Tibia.spr"
```
These match the RME `clients.toml` values.
For versions >= 960: `"extended": true`
For versions >= 1050: `"frameDurations": true`
For versions >= 1057: `"frameGroups": true`

---

## Phase 9: Update Integrations
1. `Controllers/StartupController.cpp` — `#include "Services/ClientAssetDetector.h"` (replace `ClientSignatureDetector.h`)
2. `Services/ClientVersionValidator.cpp` — same include change
3. `Services/SecondaryClientData.cpp` — same include change
4. `UI/Dialogs/Startup/StartupDialog.cpp` — remove includes to `ClientEditModal.h`, `ClientTableWidget.h`, `ClientDetailsCard.h`; any setup code that references those

---

## Phase 10: Delete Old Files
- `UI/Dialogs/ClientConfiguration/ClientEditModal.h`
- `UI/Dialogs/ClientConfiguration/ClientEditModal.cpp`
- `UI/Dialogs/ClientConfiguration/ClientTableWidget.h`
- `UI/Dialogs/ClientConfiguration/ClientTableWidget.cpp`
- `UI/Dialogs/ClientConfiguration/ClientDetailsCard.h`
- `UI/Dialogs/ClientConfiguration/ClientDetailsCard.cpp`
- `UI/DTOs/ClientEditData.h`
