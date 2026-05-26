# Plan: New Map Dialog Rewrite

## Overview

Rewrite the "New Map" dialog from scratch with no backward compatibility. Two distinct flows:

1. **Startup Dialog**: Full dialog with map metadata, client version template loader, size presets, description. Creates and saves the `.otbm` file, then returns to startup for client selection and loading.
2. **Editor-State Shortcut**: Instant unnamed map creation with same header data as the currently loaded map. No dialog, opens in new tab for cross-map copy/paste.

## Design

### Startup Dialog: New Map Dialog

```
┌─────────────────────────────────────────────────┐
│  [OTBM (Default)]  [SEC]                        │  ← tabs
├──────────────────────┬──────────────────────────┤
│  Map Name:           │  Description:            │
│  [Untitled         ] │  [                      ]│
│                      │  [                      ]│
│  Client Version:     │  [                      ]│
│  [v8.60 (860)     ▼] │  [                      ]│
│                      │                          │
│  [▸ Version Details] │                          │  ← collapsed by default
│                      │                          │
│  Map Size:           │                          │
│  [16384 × 16384   ▼] │                          │
├──────────────────────┴──────────────────────────┤
│                [Cancel]    [Create Map]          │
└─────────────────────────────────────────────────┘
```

**Tabs:**
- **OTBM (default)**: Full form as shown above
- **SEC**: "Coming soon" placeholder — greyed out, all fields disabled

**Fields:**

| Field | Type | Default | Required | Notes |
|-------|------|---------|----------|-------|
| Map Name | Text input | "Untitled" | Yes | Also used as default filename in save dialog |
| Client Version | Dropdown | None | Yes | Loads templates from `clients_templates.json`. Button disabled until selected. |
| OTBM Version | Input (hidden) | Auto-filled | Yes | Collapsed under "Version Details" expander. Editable. |
| Items Major | Input (hidden) | Auto-filled | Yes | Collapsed under "Version Details" expander. Editable. |
| Items Minor | Input (hidden) | Auto-filled | Yes | Collapsed under "Version Details" expander. Editable. |
| Map Size | Preset dropdown + Width/Height inputs | 16384×16384 | Yes | Width/Height editable when "Custom" selected |
| Description | Multi-line text area | Empty | No | OTBM description attribute |

**Size Presets:** 256, 512, 1024, 2048, 4096, 8192, 16384 (default), 32768, Custom

**Template Loading:** When user selects a Client Version template, auto-fill:
- OTBM Version (highest from `tpl.otbm_versions` if multiple)
- Items Major (`tpl.otb_major`)
- Items Minor (`tpl.otb_id`)

**Create Map Flow (Option B):**
1. User fills form → clicks "Create Map"
2. Save dialog appears → user picks `.otbm` file location
3. Map created with metadata, saved to disk via `OtbmWriter`
4. Dialog closes → user returns to startup dialog
5. Map appears in recent maps list
6. User selects it → picks client → clicks Load Map

### Editor-State Shortcut (Ctrl+N / MenuBar / Ribbon)

- **No dialog** — instant action
- Copies **all header data** from the currently loaded map:
  - OTBM version
  - Items major version
  - Items minor version
  - Map width/height
  - Description
  - Spawn file name
  - House file name
- **No tile data** copied
- Named `unnamed.otbm`, then `unnamed-2.otbm`, `unnamed-3.otbm`...
  - Monotonic counter per session (never reuses, resets on app restart)
- **Not saved to disk** until user explicitly saves
- Opens in a **new tab** alongside the current map

## Files to Modify

| File | Change |
|------|--------|
| `UI/Dialogs/NewMapDialog.h/.cpp` | Complete rewrite — new layout, tabs, template loading |
| `UI/Panels/NewMapPanel.h/.cpp` | Complete rewrite or remove (absorb into dialog) |
| `Application/MapOperationHandler.h/.cpp` | New method: `createAndSaveNewMap()`. Modify `handleNewMapDirect()` for editor-state shortcut. |
| `Services/Map/MapLoadingService.h/.cpp` | Expand `NewMapConfig` struct. Update `createNewMap()`. |
| `Application/CallbackMediator.cpp` | Update editor-state New Map wiring to skip dialog |
| `Presentation/MainWindow.h/.cpp` | Update `showNewMapDialog()` for editor-state shortcut |
| `Controllers/StartupController.h/.cpp` | Update `handleNewMapFlow()` / `handleNewMapConfirmed()` |
| `Core/Config.h` | Add size preset constants, update dialog dimensions |
| `Domain/ChunkedMap.h` | Update `createNew()` to accept full header config |

**Reference files (read-only):**
- `UI/Dialogs/ClientConfiguration/ClientPropertyEditor.cpp` — template dropdown pattern
- `Services/ClientVersionRegistry.h` — `ClientTemplate` struct
- `IO/Otbm/OtbmWriter.h/.cpp` — OTBM header structure, `write()` API
- `IO/Otbm/OtbmReader.h` — `OtbmVersionInfo` struct

## Implementation Tasks

### Task 1: Expand `NewMapConfig` and update `ChunkedMap::createNew`

**Files:** `Services/Map/MapLoadingService.h`, `Domain/ChunkedMap.h/.cpp`

Expand `NewMapConfig`:
```cpp
struct NewMapConfig {
    std::string map_name;
    uint16_t map_width;
    uint16_t map_height;
    uint32_t otbm_version;
    uint32_t items_major;
    uint32_t items_minor;
    std::string description;
};
```

Update `ChunkedMap::createNew()` to accept and store all header fields in `MapVersion`.

### Task 2: Rewrite `NewMapDialog` and `NewMapPanel`

**Files:** `UI/Dialogs/NewMapDialog.h/.cpp`, `UI/Panels/NewMapPanel.h/.cpp`

- Implement two-column layout with tabs
- Implement Client Version dropdown (template loader from `ClientVersionRegistry`)
- Implement "Version Details" collapsible section
- Implement size presets dropdown + editable Width/Height
- Implement description multi-line text area
- Implement validation (map name required, client version required)
- Implement `on_confirm_` callback with full `NewMapConfig`

### Task 3: Update `MapOperationHandler`

**File:** `Application/MapOperationHandler.h/.cpp`

Add new method `createAndSaveNewMap(config, path)`:
1. Creates `ChunkedMap` with config data
2. Saves to disk via `OtbmWriter` (pass null for `client_data` — empty map has no items)
3. Adds to recent maps via `config_.addRecentFile()` and `recent_locations_.addRecentMap()`
4. Does NOT load into editor

Modify `handleNewMapDirect()` for editor-state shortcut:
- Accept full `NewMapConfig` (not just name/width/height)
- Generate "unnamed(N).otbm" name
- Copy header data from current map if requested

### Task 4: Update `MapLoadingService::createNewMap`

**File:** `Services/Map/MapLoadingService.cpp`

Update to accept expanded `NewMapConfig` and set all header fields on the new map.

### Task 5: Implement editor-state shortcut

**Files:** `Application/CallbackMediator.cpp`, `Presentation/MainWindow.h/.cpp`, `Controllers/HotkeyController.cpp`

- Wire Ctrl+N / MenuBar / Ribbon to new instant-creation behavior
- No dialog shown
- Copy header from current map session
- Create map with "unnamed(N).otbm" name
- Open in new tab via `MapTabManager::openMap()`

Add unnamed counter to `MapTabManager`:
```cpp
static uint32_t s_next_unnamed_number = 1;
```

### Task 6: Update `StartupController`

**File:** `Controllers/StartupController.h/.cpp`

- Update `handleNewMapFlow()` to show the new dialog
- Update `handleNewMapConfirmed()` to call `createAndSaveNewMap()` instead of `handleNewMapDirect()`

### Task 7: Update `Config.h`

**File:** `Core/Config.h`

- Add size preset array/enum
- Update dialog dimensions for new layout (wider for two-column)

## Verification

- [ ] Startup: "New Map" opens new dialog with correct layout
- [ ] Template dropdown prefills OTBM Version, Items Major, Items Minor
- [ ] "Version Details" expander collapses/expands correctly
- [ ] Size presets fill Width/Height; Custom allows free input
- [ ] "Create Map" opens save dialog, creates file, returns to startup
- [ ] Created map appears in recent maps list
- [ ] Loading the created map works (correct header data)
- [ ] SEC tab shows "Coming soon" placeholder
- [ ] Editor: Ctrl+N creates unnamed map instantly (no dialog)
- [ ] Editor: unnamed map has same header data as current map
- [ ] Editor: unnamed map opens in new tab
- [ ] Editor: counter increments correctly (unnamed, unnamed-2, unnamed-3...)
- [ ] Editor: counter never reuses numbers within a session
- [ ] Validation: "Create Map" disabled until name and client version provided
- [ ] No backward compatibility with old NewMapPanel state
