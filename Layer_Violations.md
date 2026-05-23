# Layer Dependency Violations

> **Generated**: 2026-05-24 — from Architecture_1.md verification against codebase.
> Each violation lists the offending file(s), why it breaks the layer contract, and a concrete resolution path.

---

## 1. Rendering → UI (Forbidden: `Rendering must not know about ImGui panels`)

### 1.1. RenderOrchestrator includes 12+ UI headers

**Files**: `Rendering/Frame/RenderOrchestrator.cpp` (lines 19-31)
**Includes**:

```cpp
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Dialogs/Startup/StartupDialog.h"
#include "UI/Dialogs/UnsavedChangesModal.h"
#include "UI/Map/MapPanel.h"
#include "UI/Panels/BrushSizePanel.h"
#include "UI/Ribbon/Panels/FilePanel.h"
#include "UI/Ribbon/RibbonController.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "UI/Widgets/TilesetWidget.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/MinimapWindow.h"
```

**Resolution**:
`RenderOrchestrator` is already a thin "knows everything" orchestrator — by nature it must stitch UI and rendering together. However, its location in `Rendering/Frame/` creates a misleading dependency arrow. Two options:

**(Option A — Move the orchestrator)** Move `RenderOrchestrator` to `Application/` or `Presentation/`. This is the correct layer for a class that coordinates both UI and rendering. The file already behaves like a Presentation-layer component.

**(Option B — Interface-based approach)** Create an `IRenderable` interface in a shared header that each UI component implements. `RenderOrchestrator` depends only on the interface, not concrete UI types. Each UI class registers itself via `overlay_manager_->registerOverlay(...)` or a similar callback.

**Recommended**: Option A (move to `Application/Frame/RenderOrchestrator.h`). This is a 1-file move with no API changes.

---

### 1.2. StatusOverlay depends on MapViewCamera

**File**: `Rendering/Overlays/StatusOverlay.h` line 6:
```cpp
#include "UI/Map/MapViewCamera.h"
```

**Resolution**: `MapViewCamera` implements `Domain::ICoordinateTransformer`. Change the include to:
```cpp
#include "Domain/ICoordinateTransformer.h"
```
And have `StatusOverlay` depend on `ICoordinateTransformer*` instead of the concrete `MapViewCamera`. The actual `MapViewCamera` instance is already passed via `RenderOrchestrator` at call time — just change the parameter type.

**Effort**: Trivial (< 5 lines changed).

---

### 1.3. SelectionOverlay depends on MapViewCamera

**File**: `Rendering/Overlays/SelectionOverlay.h` line 4:
```cpp
#include "UI/Map/MapViewCamera.h"
```

**Resolution**: Identical fix to 1.2. Change to `#include "Domain/ICoordinateTransformer.h"` and use the interface type.

**Effort**: Trivial (< 5 lines changed).

---

## 2. Domain → Services (Forbidden: `Domain is pure data, no external deps`)

### 2.1. MapInstance depends on SelectionService

**File**: `Domain/MapInstance.h` line 4:
```cpp
#include "Services/Selection/SelectionService.h"
```

`MapInstance` has a direct member `SelectionService selection_service_` (not even a pointer).

**Resolution**: `SelectionService` is the wrong layer. The Domain should own only data, not services. Extract the pure-data parts of selection into `Domain/Selection/SelectionBucket` (already exists), then:

1. Move `SelectionService` to `Services/Selection/` (it's already there — just stop embedding it in `MapInstance`).
2. `MapInstance` stores only `SelectionBucket` as the domain state.
3. `SelectionService` becomes a session-scoped service that takes `SelectionBucket&` by reference and provides the operations on it.
4. All current callers of `mapInstance->getSelectionService()` change to receive a `SelectionService*` via constructor injection instead.

**Effort**: Medium (affects ~10-15 call sites).

---

### 2.2. History classes depend on Services

**Files**:
- `Domain/History/HistoryManager.cpp` → `#include "Services/Selection/SelectionService.h"`
- `Domain/History/HistoryEntry.cpp` → `#include "Services/ClientDataService.h"` + `#include "Services/Selection/SelectionService.h"`

**Resolution**: These are likely used for undo/redo of selection state or client-version-aware serialization. Solutions:

1. **Selection dependency**: Extract the selection state capture/restore into a free function in `Services/Selection/` that the caller invokes *before* calling `HistoryManager::endOperation()`. The history system shouldn't know about selection.
2. **ClientDataService dependency**: If this is for version-specific tile serialization, move the serialization logic into `IO/TileSnapshotCodec` (which already handles LZ4) and pass the version info as a parameter rather than looking it up from a service.

**Effort**: Medium (refactor touches 2 files in Domain, 1-2 in Services).

---

### 2.3. Domain files including ConfigService

**File**: `Domain/SelectionSettings.cpp` line 2:
```cpp
#include "Services/ConfigService.h"
```

**Resolution**: `SelectionSettings` is a struct of pure configuration values (bool, int, etc.). It shouldn't know how to persist itself. Move the save/load logic into `Services/ConfigService` itself — let it serialize/deserialize the struct. The Domain struct becomes a plain data type.

**Effort**: Low (move ~10 lines of serialization code).

---

### 2.4. MapInstance.cpp includes ClientDataService

**File**: `Domain/MapInstance.cpp` line 2:
```cpp
#include "../Services/ClientDataService.h"
```

**Resolution**: This is the most concerning — it uses a relative `../` include which is a code smell. Identify what `MapInstance` needs from `ClientDataService` and either:
- Pass that data as a parameter to the method that needs it.
- If it's version-specific map initialization, move that logic to `Services/Map/MapLoadingService`.

**Effort**: Medium (requires understanding the specific usage).

---

## 3. Services → UI (Forbidden: `Services never call ImGui`)

### 3.1. AppSettings depends on Theme

**File**: `Services/AppSettings.cpp` line 3:
```cpp
#include "UI/Core/Theme.h"
```

**Resolution**: Theme colors are presentation data. Two options:

**(Option A)** Move color constants into a `Core/ThemeColors.h` header at the Core layer (not UI). Both `UI/Core/Theme.h` and `Services/AppSettings.cpp` include it.

**(Option B)** `AppSettings` stores colors as raw `float[4]` arrays. `UI/Core/Theme.h` includes `AppSettings` to apply persisted colors to the ImGui style system. The dependency arrow reverses to the correct direction (UI → Services).

**Recommended**: Option B — move the color application logic to UI where it belongs.

**Effort**: Low.

---

## 4. Services → Rendering (Forbidden: `Services don't issue GL calls`)

### 4.1. SpriteManager depends on Rendering types

**File**: `Services/SpriteManager.h`:
```cpp
#include "Rendering/Core/Texture.h"
#include "Rendering/Overlays/OverlaySpriteCache.h"
#include "Rendering/Resources/AtlasManager.h"
#include "Rendering/Resources/SpriteAtlasLUT.h"
```

**Resolution**: This is the hardest violation to fix because `SpriteManager` IS the bridge between async I/O and GPU uploads. The current architecture correctly keeps GL calls in `Rendering/` but needs `SpriteManager` to orchestrate the pipeline.

**(Option A — Split into two classes)**:
- `Services/SpriteDataService` — owns sprite data, decompression, caching. Lives in Services. Depends only on Domain + I/O.
- `Rendering/SpriteGpuUploader` — owns PBO uploads, atlas management. Lives in Rendering. Takes raw sprite data from `SpriteDataService` and uploads.

**(Option B — Accept the violation)**:
This is a legitimate "bridge" service. The `SpriteManager` doesn't issue GL calls itself — it delegates to `SpriteAsyncLoader` and `AtlasManager` (both in Rendering). The includes are for type definitions only, not for calling GL functions. Document this as an intentional architectural exception rather than a violation.

**Recommended**: Option B for now (mark as intentional exception). Option A is a larger refactor for later.

**Effort**: Option B = trivial (document); Option A = large (2-3 day refactor).

---

### 4.2. Other Services → Rendering includes

**Files**:
- `Services/SpriteAsyncLoader.h` → `Rendering/Core/PixelBufferObject.h`, `Rendering/Resources/AtlasManager.h`
- `Services/ItemCompositor.h` → `Rendering/Core/Texture.h`
- `Services/CreatureSpriteService.h` → `Rendering/Core/Texture.h`, `Rendering/Resources/AtlasManager.h`

**Resolution**: Same diagnosis as 4.1 — these are the GPU-asset bridge layer. `SpriteAsyncLoader` is the designated boundary between CPU and GPU. `ItemCompositor` and `CreatureSpriteService` compose sprites that will be uploaded to the GPU.

**Recommendation**: Mark as intentional architectural exceptions. These services exist specifically to feed data into the Rendering pipeline. They do not issue `gl*` calls themselves.

---

## 5. I/O → Services (Forbidden: `I/O reads/writes data, doesn't call business logic`)

### 5.1. I/O files depending on ClientDataService

**Files** (6 total):
- `IO/SecReader.cpp`
- `IO/Sec/SecTileParser.cpp`
- `IO/Sec/SecItemParser.cpp`
- `IO/Otbm/OtbmWriter.cpp`
- `IO/Otbm/OtbmReader.cpp`
- `IO/Otbm/OtbmItemParser.cpp`
- `IO/Otbm/OtbmIdConverter.cpp`

All include `#include "Services/ClientDataService.h"`.

**Resolution**: `ClientDataService` provides item/metadata lookups needed during parsing (e.g., resolving server IDs to client IDs). This is a data access concern, not business logic. Two approaches:

**(Option A — Extract ID mapping into Domain)** Create `Domain/ItemTypeDatabase` as a pure data container loaded at startup. I/O readers query it directly. `ClientDataService` wraps it for higher layers.

**(Option B — Pass the lookup as a parameter)** Change I/O readers to accept `const ClientDataService*` or a narrower interface as a constructor/call parameter. The dependency becomes "I/O uses externally-provided data" rather than "I/O owns a service dependency."

**Recommended**: Option B. It follows the existing pattern of dependency injection and is minimal effort.

**Effort**: Low (change includes to forward declarations, add constructor parameter).

---

### 5.2. BrushXmlReader depends on lookup services

**File**: `IO/BrushXmlReader.cpp`:
```cpp
#include "Services/Brushes/BorderLookupService.h"
#include "Services/Brushes/CarpetLookupService.h"
#include "Services/Brushes/TableLookupService.h"
#include "Services/Brushes/WallLookupService.h"
```

**Resolution**: Same diagnosis as 5.1. The lookup services provide data for XML parsing. Accept them as parameters or extract the pure lookup data into domain types.

**Effort**: Low.

---

### 5.3. HotkeyJsonReader depends on HotkeyRegistry

**File**: `IO/HotkeyJsonReader.cpp`:
```cpp
#include "Services/HotkeyRegistry.h"
```

**Resolution**: `HotkeyRegistry` is a service that combines data storage with runtime lookup. Split it:
- `Domain/HotkeyDefinitions` — pure data.
- `Services/HotkeyRegistry` — runtime activation logic.

The JSON reader depends only on `HotkeyDefinitions`.

**Effort**: Low-Medium.

---

## 6. Application.cpp Has Business Logic (AGENTS.md violation)

### 6.1. Non-trivial logic in Application.cpp

**File**: `Application.cpp` (399 lines, beyond "init and main loop wiring")

Problematic methods:
- `onMapLoaded()` (lines 207-230) — session wiring, UI sync, state transitions
- `wireCallbacks()` (lines 123-205) — 80+ lines of inter-component wiring
- `createVersionCoordinator()` (lines 247-259) — factory logic

**Resolution**:
1. Move `wireCallbacks()` body into `CallbackMediator::wireAll()` — the `Context` struct is already populated by Application, but the 80-line wiring should live in `CallbackMediator.cpp`.
2. Move `onMapLoaded()` logic into `SessionWiringService` — it already handles the main wiring; Application should just delegate.
3. `createVersionCoordinator()` → move to `Application/ClientVersionManager` as a static factory or to a dedicated `Application/VersionSwitchCoordinator` class (already exists at `Application/Coordination/`).

**Effort**: Medium (touches 3 areas, each well-contained).

---

## 7. Controllers → UI (Architecturally Suspicious)

### 7.1. Controllers including UI headers

**Files**:
- `Controllers/StartupController.h` → `#include "UI/Dialogs/Startup/StartupDialog.h"`
- `Controllers/SearchController.h` → `#include "UI/Widgets/QuickSearchPopup.h"`, `UI/Dialogs/AdvancedSearchDialog.h`, `UI/Widgets/SearchResultsWidget.h"`

**Resolution**: Controllers should emit abstract events/state changes that UI observes, not directly reference UI classes. Current workaround: if these are forward declarations for pointer/reference members only, change `#include` to forward declarations. If controllers call UI methods directly, extract an interface.

For `SearchController`: The controller likely triggers search results display. Instead of holding a `SearchResultsWidget*`, the controller should expose results via a `std::function<...> onResultsChanged` callback. The UI subscribes to it.

**Effort**: Low (mostly changing includes to forward declarations).

---

## Summary by Priority

| # | Violation | Files Affected | Effort | Priority |
|---|-----------|---------------|--------|----------|
| 1.2 | StatusOverlay → MapViewCamera | 1 | Trivial | 🟢 |
| 1.3 | SelectionOverlay → MapViewCamera | 1 | Trivial | 🟢 |
| 3.1 | AppSettings → Theme | 2 | Low | 🟢 |
| 5.3 | HotkeyJsonReader → HotkeyRegistry | 2 | Low | 🟢 |
| 7.1 | Controllers → UI headers | 2 | Low | 🟢 |
| 5.1 | I/O → ClientDataService | 7 | Low | 🟡 |
| 5.2 | BrushXmlReader → lookup services | 1 | Low | 🟡 |
| 2.3 | SelectionSettings → ConfigService | 1 | Low | 🟡 |
| 2.1 | MapInstance → SelectionService | 10-15 | Medium | 🟡 |
| 2.2 | History → Services | 3-4 | Medium | 🟡 |
| 2.4 | MapInstance → ClientDataService | 1 | Medium | 🟡 |
| 6.1 | Application.cpp logic | 3 areas | Medium | 🟡 |
| 1.1 | RenderOrchestrator → 12 UI headers | 1 move | Low (move file) | 🟡 |
| 4.1 | SpriteManager → Rendering | Bridge layer | Document as exception | 🔵 |
| 4.2 | Other Services → Rendering | Bridge layer | Document as exception | 🔵 |

**🟢 = Quick win, do now.  🟡 = Planned refactor.  🔵 = Intentional exception (document, don't fix).**

---

## Quick Wins (5 files, ~15 lines changed)

If you fix only these 3 violations today, you eliminate the most egregious layer breaks:

1. `Rendering/Overlays/StatusOverlay.h` — change `#include "UI/Map/MapViewCamera.h"` → `#include "Domain/ICoordinateTransformer.h"`
2. `Rendering/Overlays/SelectionOverlay.h` — same change
3. `Services/AppSettings.cpp` — move `Theme.h` dependency to UI side
4. `Controllers/StartupController.h` — change includes to forward declarations
5. `Controllers/SearchController.h` — change includes to forward declarations
