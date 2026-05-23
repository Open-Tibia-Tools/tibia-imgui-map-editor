# C++ Project Guidelines - Architecture & Style

<critical_rules>

## NO CODE IN APPLICATION.CPP
- Application.cpp is ONLY for initialization and main loop wiring
- If you're about to add business logic there — STOP and create a proper module

## SEARCH BEFORE YOU CODE
Before writing ANY new function, class, or logic block:
1. Search the codebase for similar functionality
2. Check if this logic already exists (even partially)
3. Look for utility functions that do related tasks
4. Verify no duplicate implementations exist

IF SIMILAR CODE EXISTS — REUSE IT, DON'T DUPLICATE IT

## NO DUPLICATE CODE
- If you find yourself copying logic, create a shared function instead
- If similar patterns exist in 2+ places, refactor to shared utility
- DRY is NOT optional

## SIZE LIMITS
- Function exceeds 150 lines → refactor
- File exceeds 500 lines → split

</critical_rules>

---

<architecture>

<raii>
- Never manage `GLuint` handles manually
- Every OpenGL object (VAO, VBO, Shader, Texture) must be wrapped in a class that handles `glDelete*` in its destructor
- No raw `new`/`delete` for GL resources
</raii>

<ownership>
- Use `std::unique_ptr` for sole ownership
- Use `std::shared_ptr` only when ownership is truly shared (e.g., a Texture asset used by multiple renderers)
- Use raw pointers (`T*`) or references (`T&`) only as non-owning observers — never to manage lifetime
</ownership>

<const_correctness>
- Aggressively mark methods `const` if they do not modify internal state
- Prefer `const&` for input parameters of non-trivial types
</const_correctness>

<layering>
- Dependencies flow downwards only: UI → Controllers → Core → Services → Domain → I/O → Platform
- Never leak ImGui code into Core, Services, or Rendering layers
- Never let Domain or Rendering know about UI
- Prefer simple structs and free functions over deep class hierarchies until complexity demands otherwise
</layering>

<no_global_state>
- No singletons, no global variables
- Pass dependencies explicitly via constructor injection
- If you need something everywhere, it belongs in a service injected at construction — not a global
</no_global_state>

<single_responsibility>
- Each class owns one concept
- Each module has a clear, single responsibility with its own `.cpp`/`.h` pair
- No logic bleeding across layer boundaries
</single_responsibility>

</architecture>

---

<workflow>

Before writing any code:
1. UNDERSTAND the requirement fully
2. SEARCH for existing implementations (`grep -r 'thing' src/`)
3. DECIDE which existing module owns this — check README for module map
4. CHECK if a new file is needed or if an existing one should be extended
5. IMPLEMENT in the right place
6. VERIFY no duplication was introduced
7. UPDATE relevant headers

If similar code found:
- Can I use it directly? → Use it
- Can I extend it? → Extend it, don't duplicate
- Is it in the wrong place? → Refactor once, then use it
- Is it slightly different? → Parameterize to handle both cases

</workflow>

---

<file_organization>

- One class per file, unless classes are tightly coupled small helpers
- Headers: `.h`, implementations: `.cpp`
- Always use `#pragma once`
- Use forward declarations liberally in headers to reduce compile times and avoid circular dependencies

### Include Order
1. Precompiled header (if applicable)
2. Corresponding `.h` file
3. C++ Standard Library (`<vector>`, `<algorithm>`, `<format>`)
4. Third-party libs (`<glad/glad.h>`, `<imgui.h>`, `<glm/glm.hpp>`)
5. Project headers (`"core/Log.h"`)

</file_organization>

---

<modern_cpp>

### Types & Pointers
- Use `auto` for iterators and complex template types, or when the type is obvious from the RHS
- Be explicit with numeric types: use `int32_t`, `uint64_t`, `size_t`, `float` — avoid bare `int` or `unsigned` in data structures
- Always use `nullptr`, never `NULL` or `0`

### Containers & Views
- Use `std::span<T>` instead of pointer + size pairs: `void Upload(std::span<const uint8_t> data);`
- Use `std::string_view` for read-only string arguments to avoid allocations

### Concepts
- Prefer C++20 `requires` clauses over SFINAE for constrained templates:
  ```cpp
  template<typename T>
  requires std::integral<T>
  void SnapToGrid(T& value, T gridSize);
  ```

</modern_cpp>

---

<graphics_and_ui>

### OpenGL
- All GL calls must happen on the main thread — no exceptions
- Use `glm` for all math. Pass `glm::vec2`/`glm::vec3` by value, `glm::mat4` by `const&`
- Do not make redundant GL state changes — cache state or use a stateless renderer abstraction that sorts draw calls
- Build vertex data in `std::vector`, upload to GPU via `glBufferData` — never build on the GPU side directly
- Check GL errors in debug builds: wrap calls with error checking in non-release configurations
- Every shader must be compiled, linked, and validated at startup — fail loudly, not silently

### RAII for GL Objects
```cpp
// Correct — destructor calls glDeleteVertexArrays
class VertexArray {
public:
    VertexArray()  { glGenVertexArrays(1, &id_); }
    ~VertexArray() { glDeleteVertexArrays(1, &id_); }
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
private:
    GLuint id_ = 0;
};
```

### Render Passes
- Each render pass has a single responsibility — terrain, lighting, overlays are separate passes
- Passes must not reach into domain data directly — receive only what they need via parameters or a render context struct
- New overlays implement `IOverlayRenderer` and register via `OverlayManager`, not hardcoded into the pipeline

### Dear ImGui
- In loops, always use `ImGui::PushID(index)` / `ImGui::PopID()` — never skip this
- Use `const char*` literals where possible; `std::format` for dynamic labels
  - Good: `ImGui::Text("Pos: %d, %d", x, y);`
  - Good: `ImGui::TextUnformatted(std::format("Count: {}", count).c_str());`
- UI panels never talk to Domain directly — always go through a Service or Controller
- Never put ImGui calls inside Rendering or Domain layer code

</graphics_and_ui>

---

<error_handling>

- No silent failures — if something can't be initialized, throw or assert loudly at startup
- GL errors: use a debug callback (`glDebugMessageCallback`) in debug builds; do not poll `glGetError()` in hot paths
- File I/O failures must surface as descriptive errors, not empty/corrupt state
- Avoid bare `catch(...)` — catch specific types and handle or rethrow
- Do not use exceptions for control flow — only for genuinely exceptional conditions
- Failed resource loads (shaders, textures) must leave the object in a well-defined invalid state, not half-initialized

</error_handling>

---

<threading>

- All OpenGL calls: main thread only, no exceptions
- All ImGui calls: main thread only
- Background threads are permitted only for: asset loading, file I/O, compression
- Shared data between threads must be protected — use `std::mutex` or atomics; document the lock ownership clearly
- Never pass raw GL handles (VAO, VBO, texture IDs) to background threads
- Background thread results must be handed back to main thread before any GL upload occurs

</threading>

---

<documentation>

- Doxygen style for all public API in `.hpp` files
- Comment complex algorithms inline in `.cpp` — explain *why*, not *what*
- TODOs: `// TODO(Name): Description`
- Do not leave commented-out code in PRs — delete it or track it as a TODO

</documentation>

<architecture>

# Architecture Encyclopedia

> **Purpose**: This document is the single-source-of-truth blueprint for any coding agent working on this codebase. Read this BEFORE writing any code. It answers "when to edit which file", documents every contract, and maps every dependency.

---

## Table of Contents

- [1. Coordinate System Contract](#1-coordinate-system-contract)
  - [1.1. The Three Spaces](#11-the-three-spaces)
  - [1.2. Conversion Functions & Owners](#12-conversion-functions--owners)
  - [1.3. The Input Pipeline (Mouse → Tile)](#13-the-input-pipeline-mouse--tile)
  - [1.4. Key Constants](#14-key-constants)
- [2. Threading Model](#2-threading-model)
  - [2.1. Thread Ownership Rules](#21-thread-ownership-rules)
  - [2.2. SpriteAsyncLoader Pipeline](#22-spriteasyncloader-pipeline)
  - [2.3. The `process()` Frame-Tick Contract](#23-the-process-frame-tick-contract)
  - [2.4. PBO — The GPU Handoff Boundary](#24-pbo--the-gpu-handoff-boundary)
  - [2.5. Adding a New Async System](#25-adding-a-new-async-system)
- [3. Session-Scoped vs App-Scoped Services](#3-session-scoped-vs-app-scoped-services)
  - [3.1. App-Scoped Services (Singleton Lifetime)](#31-app-scoped-services-singleton-lifetime)
  - [3.2. Session-Scoped State (Per-Map Lifetime)](#32-session-scoped-state-per-map-lifetime)
  - [3.3. EditorSession Anatomy](#33-editorsession-anatomy)
  - [3.4. MapTabManager & Shared State](#34-maptabmanager--shared-state)
  - [3.5. SessionLifecycleManager](#35-sessionlifecyclemanager)
  - [3.6. Pattern: App Service Operating on Session](#36-pattern-app-service-operating-on-session)
- [4. UI → Service Wiring Patterns](#4-ui--service-wiring-patterns)
  - [4.1. UIFactory & UIFactoryContext](#41-uifactory--uifactorycontext)
  - [4.2. The Three Wiring Patterns](#42-the-three-wiring-patterns)
  - [4.3. Concrete Wiring Examples](#43-concrete-wiring-examples)
  - [4.4. Decision Tree: Which Pattern to Use](#44-decision-tree-which-pattern-to-use)
  - [4.5. DialogContainer](#45-dialogcontainer)
- [5. CallbackMediator — Event System](#5-callbackmediator--event-system)
  - [5.1. All Event Types](#51-all-event-types)
  - [5.2. When to Use CallbackMediator vs Direct Calls](#52-when-to-use-callbackmediator-vs-direct-calls)
  - [5.3. Publishing & Subscribing Patterns](#53-publishing--subscribing-patterns)
- [6. Rendering Pipeline](#6-rendering-pipeline)
  - [6.1. Per-Frame Sequence](#61-per-frame-sequence)
  - [6.2. Chunk Caching System](#62-chunk-caching-system)
  - [6.3. The Golden Rendering Rule](#63-the-golden-rendering-rule)
  - [6.4. SpriteBatch & Batching Strategy](#64-spritebatch--batching-strategy)
  - [6.5. Atlas System (SpriteAtlas / AtlasManager)](#65-atlas-system-spriteatlas--atlasmanager)
  - [6.6. Overlay System](#66-overlay-system)
  - [6.7. RenderState (Per-Session)](#67-renderstate-per-session)
  - [6.8. Shader Inventory](#68-shader-inventory)
  - [6.9. Performance Rules for the Render Loop](#69-performance-rules-for-the-render-loop)
- [7. Brush System — Status & Boundaries](#7-brush-system--status--boundaries)
  - [7.1. Architecture](#71-architecture)
  - [7.2. Stability Status Per Brush Type](#72-stability-status-per-brush-type)
  - [7.3. What's Broken & Why](#73-whats-broken--why)
  - [7.4. Safe to Build On vs Needs Rewriting](#74-safe-to-build-on-vs-needs-rewriting)
- [8. Stability Tiers — What to Touch, What to Avoid](#8-stability-tiers--what-to-touch-what-to-avoid)
  - [8.1. 🟢 STABLE — Safe to Build On](#81--stable--safe-to-build-on)
  - [8.2. 🟡 PARTIAL — Use with Caution](#82--partial--use-with-caution)
  - [8.3. 🔴 NOT FUNCTIONAL — Do Not Build On](#83--not-functional--do-not-build-on)
- [9. Application Lifecycle](#9-application-lifecycle)
  - [9.1. Startup Sequence](#91-startup-sequence)
  - [9.2. Main Loop](#92-main-loop)
  - [9.3. Shutdown Sequence](#93-shutdown-sequence)
- [10. Layer Dependency Rules](#10-layer-dependency-rules)
  - [10.1. Layer Stack](#101-layer-stack)
  - [10.2. Allowed Dependencies](#102-allowed-dependencies)
  - [10.3. Forbidden Dependencies](#103-forbidden-dependencies)
- [11. Directory Ownership — When to Edit Which File](#11-directory-ownership--when-to-edit-which-file)
  - [11.1. By Feature Type](#111-by-feature-type)
  - [11.2. Full Directory Map](#112-full-directory-map)
- [12. History System (Undo/Redo)](#12-history-system-undoredo)
  - [12.1. Architecture](#121-architecture)
  - [12.2. Recording an Undoable Operation](#122-recording-an-undoable-operation)
- [13. I/O Layer](#13-io-layer)
  - [13.1. Binary Formats](#131-binary-formats)
  - [13.2. XML Formats](#132-xml-formats)
  - [13.3. DAT Reader Hierarchy](#133-dat-reader-hierarchy)
- [14. Service Wiring Checklist](#14-service-wiring-checklist)
  - [14.1. Creating a New Service](#141-creating-a-new-service)
  - [14.2. Creating a New Controller](#142-creating-a-new-controller)
  - [14.3. Creating a New UI Panel](#143-creating-a-new-ui-panel)
  - [14.4. Creating a New Overlay](#144-creating-a-new-overlay)
  - [14.5. Creating a New Dialog](#145-creating-a-new-dialog)

---

## 1. Coordinate System Contract

### 1.1. The Three Spaces

```text
┌──────────────────────────────────────────────────────────────────────┐
│  TILE SPACE          WORLD SPACE              SCREEN SPACE          │
│  (Position)          (pixels)                 (viewport pixels)     │
│                                                                      │
│  (5, 10, 7)    →    (160.0, 320.0)     →    (412.0, 287.0)        │
│  integer tiles       ×32 each               camera + zoom          │
│  x,y,z              float pixels            float viewport px      │
└──────────────────────────────────────────────────────────────────────┘
```

| Space | Unit | Origin | Used By |
|-------|------|--------|---------|
| **Tile space** | `Domain::Position{x, y, z}` — integer tile coordinates | `(0,0,0)` top-left, floor 0 | Domain, Services, Brushes |
| **World space** | `float` pixels — `tile * TILE_SIZE` | Same as tile but scaled by 32.0f | Rendering layer (shaders, batching) |
| **Screen space** | `float` pixels — ImGui/window coordinates | `(0,0)` = top-left of viewport | UI layer (MapPanel, input handling) |

> [!IMPORTANT]
> **z=0 is the highest floor** (surface). z=7 is ground level. z=8–15 are underground. This is Tibia convention, not standard graphics convention.

### 1.2. Constants — Single Source of Truth

All constants live in `Core/Config.h`:

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| `TILE_DIMENSION` | `32` | `Config::Rendering` | Base integer tile dimension |
| `TILE_SIZE` | `32.0f` | `Config::Rendering` | Float version of tile size |
| `GROUND_LAYER` | `7` | `Config::Map` | Ground floor level |
| `MIN_FLOOR` / `MAX_FLOOR` | `0` / `15` | `Config::Map` | Floor limits |

### 1.3. Three Parallel Transform Implementations

> [!CAUTION]
> **CRITICAL**: There are three separate coordinate transform implementations. Know which one to use.

| Class | File | Layer | Purpose |
|-------|------|-------|---------|
| `MapViewCamera` | `UI/Map/MapViewCamera.h` | UI | Primary transform for mouse input → tile. Implements `ICoordinateTransformer` |
| `ViewCamera` | `Rendering/Camera/ViewCamera.h` | Rendering | Render-layer camera. Produces `view_matrix_` for shaders. |
| `CoordUtils::tileToScreen()` | `Rendering/Utils/CoordUtils.h` | Rendering | Stateless free function for overlay/tooltip positioning |

`MapViewCamera` implements the `ICoordinateTransformer` interface. Services and controllers that need coordinate transforms should depend on `ICoordinateTransformer*`, not concrete classes.

### 1.4. Floor Parallax

Above-ground floors (z < 7) have a diagonal parallax offset, applying the formula:
`floor_offset = float(GROUND_LAYER - pos.z) * TILE_SIZE * zoom`
This shifts upper floors diagonally up-left. Both `MapViewCamera` and `ViewCamera` must apply this same offset.

### 1.5. The Input Pipeline (Mouse → Tile)

```text
Mouse click (screen pixels)
    → MapPanelInput captures ImGui mouse pos
    → MapViewCamera::screenToTile() → Domain::Position
    → MapInputController receives tile position
    → Dispatches to MapEditingService / BrushSystem / SelectionService
```

**Rule**: Screen → Tile conversion happens in `MapViewCamera` (`UI/Map/MapViewCamera.h`). The rendering layer's `ViewCamera` is **not** used for input processing, and `MapInputController` simply receives the pre-calculated tile coordinates from `MapPanelInput`.

---

## 2. Threading Model

### 2.1. Thread Ownership Rules

| Thread | Allowed Operations | Forbidden |
|--------|-------------------|-----------|
| **Main thread** | ALL OpenGL calls, ALL ImGui calls, PBO uploads, service calls | Blocking on file I/O |
| **Background thread(s)** | File I/O (.spr/.dat reading), sprite decompression | Any `gl*` call, any ImGui call, touching `GLuint` handles |

> [!CAUTION]
> **NEVER** call any `gl*` function from a background thread.
> **NEVER** pass `GLuint` handles (VAO, VBO, texture IDs, PBO IDs) to a background thread.
> Background threads produce **raw byte data** only. The main thread owns ALL GPU state.

### 2.2. Sprite Loading Pipeline

```text
Background threads (SpriteLoadQueue worker threads)
    → Decompress sprite RGBA data into CPU buffers
    → Push completed SpriteLoadResult to thread-safe queue
    ↓
Main thread (every frame)
    Application::update()
      → version_manager_.update()                    // ClientVersionManager
        → sprite_manager_->processAsyncLoads()       // SpriteManager
          → SpriteAsyncLoader::process()
            → Drain completed queue
            → Upload via PBO → glTexSubImage2D into SpriteAtlas
```

**Synchronization primitives (in `Services/SpriteLoadQueue`):**
- `std::mutex` — protects both request and completed queues
- `std::condition_variable` — wakes background thread on new requests
- `std::atomic<bool>` — shutdown signaling

> [!NOTE]
> The class name is `ClientVersionManager` (not `VersionManager`). The member in `Application` is named `version_manager_`, which is why the shorthand is used in this document.

### 2.3. The `process()` Frame-Tick Contract

> [!IMPORTANT]
> The async processing chain `version_manager_.update()` **MUST be called every frame** from the main loop's `Application::update()`. This eventually calls `SpriteAsyncLoader::process()` to drain the completed sprite queue and upload pixel data to the GPU.

**Where it's called:** `Application::run()` → `update()` → `version_manager_.update()`.

**If you add a new async system**, you MUST:
1. Create a `process()` (or equivalent) method on your loader.
2. Call it every frame from `Application::update()` or within the appropriate manager's update loop.
3. Ensure all GPU uploads happen inside `process()` on the main thread.

### 2.4. PBO — The GPU Handoff Boundary

`PixelBufferObject` is the **architectural boundary** between CPU-side async work and GPU-side uploads.
- **2 PBOs** alternate: one maps for CPU write while the other uploads to GPU.
- **Fence sync** (`glFenceSync` / `glClientWaitSync`) ensures the GPU is done before reuse.

---

## 3. Session-Scoped vs App-Scoped Services

### 3.1. App-Scoped Services (Singleton Lifetime)

Created once in `Application::initializePlatform()` or `initializeUIComponents()`, live for the entire app lifetime.
Examples include `ClientDataService`, `SpriteManager`, `ConfigService`, `MapTabManager`, `CallbackMediator`, `DialogContainer`.

### 3.2. Session-Scoped State (Per-Map Lifetime)

Created by `SessionLifecycleManager` when opening/creating a map, destroyed when closing the tab.
State directly or indirectly belonging to the tab includes `MapInstance` (and its `ChunkedMap`, `HistoryManager`, `SelectionService`), UI states like `ViewState`, `MinimapState`, and `RenderState`.

### 3.3. EditorSession Anatomy

`EditorSession` represents a per-tab editing session.

```cpp
// Application/EditorSession.h
class EditorSession {
    // === OWNED DOMAIN DOCUMENT ===
    std::unique_ptr<Domain::MapInstance> document_;
    
    // === SESSION ID (For RenderState) ===
    SessionID session_id_;
    
    // === UI & LOGIC STATES ===
    ViewState view_state_;
    MinimapState minimap_state_;
    IngamePreviewState ingame_preview_state_;
    BrowseTileState browse_tile_state_;
    Services::CreatureSimulator creature_simulator_;
    Services::Preview::PreviewService preview_service_;

    // ... API delegates to document_
};
```

> [!WARNING]
> `EditorSession` DOES NOT own `RenderState`. `RenderState` lives in `RenderingManager::session_states_` indexed by `SessionID`. This deliberately keeps GL resources out of the domain layer.

### 3.4. MapInstance — Pure Domain Document

The domain state is encapsulated in `MapInstance`:
- Owns `ChunkedMap` (the actual tile data)
- Owns `SelectionService` (what tiles are selected)
- Owns `HistoryManager` (undo/redo)
- Tracks `file_path_` and `modified_` state

### 3.5. MapTabManager & Shared State

`MapTabManager` manages multiple `EditorSession` instances.
`CopyBuffer` is shared across sessions for cross-tab copy/paste.

### 3.6. Session Lifecycle

When a new map is loaded:
1. `MapOperationHandler::loadMapFromPath()` loads the map file, fires `on_map_loaded_` callback.
2. `Application::onMapLoaded()` calls `session_wiring_->wireResources(...)`.
3. `SessionWiringService::wireResources()` orchestrates:
   - `MapTabManager::openMap(...)` — creates `MapInstance`, `EditorSession`, and GPU cache via `RenderingManager::createRenderState(session_id, client_data)`.
   - Creates renderer via `RenderingManager::createRenderer()`.
   - Wires sprite_manager callbacks.

> [!NOTE]
> The method on `MapTabManager` is `openMap()`, not `createSession()`. `SessionWiringService` is the orchestrator that drives the entire session-creation flow, not a final "connect" step.

---

## 4. UI → Service Wiring Patterns

### 4.1. UIFactory & UIFactoryContext

`UIFactory` creates all controllers and views, injecting dependencies from `UIFactoryContext` (which groups multiple service pointers):

```
Application::initializeUIComponents()
    │
    ├── Populates UIFactoryContext with pointers to all services
    │
    └── UIFactory::create(ctx)
            │
            ├── create MapPanel(ctx.view_settings)
            ├── create MapInputController(ctx.selection_settings, ...)
            ├── create RibbonController()
            ├── create WorkspaceController(...)
            └── returns UIComponentContainer
```

**Key rule:** Controllers and UI components receive **non-owning pointers** via constructor or context injection. They never own services.

### 4.2. The Three Wiring Patterns

#### Pattern A: Direct Service Call (View → Service)

```
View ──→ Service
```

For simple data queries where a controller would add no value:

```cpp
// PropertyWidgets directly displays domain data
void PropertyPanel::render() {
    auto* session = tab_manager_->getActiveSession();
    if (!session) return;
    auto& selection = session->getSelectionService();
    auto& bucket = selection.getBucket();
    // render ImGui property widgets...
}
```

> [!NOTE]
> `BrowseTileWindow` does **NOT** use Pattern A. It uses **setter injection** — `setMap()`, `setSession()`, `setSelection()` are called by `WorkspaceController::bindSession()` to provide its dependencies.

#### Pattern B: Controller-Mediated (View → Controller → Service)

```
View ──→ Controller ──→ Service
```

For user input that triggers business logic:

```cpp
// MapPanel delegates input to MapInputController
void MapPanel::processInput() {
    input_controller_->handleMouseInput(mouse_pos);
    // controller internally calls editing services
}
```

#### Pattern C: Active Session Polling (View → Active Session → Data)

```
View ──→ tab_manager_->getActiveSession() ──→ Session State
```

For UI that updates based on the currently active map tab:

```cpp
void SomePanel::render() {
    auto* session = tab_manager_->getActiveSession();
    if (!session) return;
    auto& selection = session->getSelectionService();
    // Display selection...
}
```

### 4.3. Concrete Wiring Examples

#### BrowseTileWindow (Setter Injection)

```
BrowseTileWindow
  │ Injected via setter methods:
  │   setMap(ChunkedMap*, ClientDataService*, SpriteManager*)
  │   setSession(EditorSession*)
  │   setSelection(const SelectionService*)
  │
  ├── map_->getTile(pos)                    → get tile data
  └── selection_->getSelection()            → check selection bounds
```

> [!NOTE]
> BrowseTileWindow uses **setter injection** (not active-session polling via `MapTabManager`). The bindings are established by `WorkspaceController::bindSession()`.

#### MapPanel → MapInputController (Controller-mediated)

```
MapPanel (UI)
  │ owns reference to: MapInputController*
  │
  ├── Forwards mouse events
  │     ├── controller->handleMouseInput(pos)
  │     │     └── Editing services place items / update history
  │
  └── Renders via MapRenderer (triggered by MainWindow)
```

### 4.4. Decision Tree: Which Pattern to Use

```
Is this a simple data query for display?
  YES → Direct service call from View
  NO  ↓

Does this handle user input / trigger business logic?
  YES → Controller → Service
  NO  ↓

Does this display data for the currently open map?
  YES → Active Session Polling (Pattern C)

Special cases:
  Controller → Controller? → ❌ NEVER (controllers don't know about each other)
  View → Domain directly? → ❌ NEVER (always go through a Service or Session state)
```

### 4.5. DialogContainer

```cpp
// Application/DialogContainer.h
struct DialogContainer {
    // File menu dialogs
    UI::UnsavedChangesModal unsaved_changes;
    UI::ImportMapDialog import_map;
    UI::ImportMonstersDialog import_monsters;
    UI::PreferencesDialog preferences;
    
    // Map menu dialogs
    UI::EditTownsDialog edit_towns;
    UI::MapPropertiesDialog map_properties;
    UI::ConfirmationDialog cleanup_confirm;
    
    // Dialog controllers (Presentation layer)
    Presentation::ImportMapController import_controller;
    Presentation::CleanupController cleanup_controller;
    Presentation::TownPickController town_pick_controller;
};
```

- **Owned by:** `Application` via `DialogContainer`
- **Rendered by:** `RenderOrchestrator::renderDialogs()`
- **Pattern:** DialogContainer is a simple aggregate struct of stack instances. Dialogs are rendered sequentially each frame by the orchestrator.

---

## 5. CallbackMediator — One-Time Wiring Helper

### 5.1. All Wiring Connections

> [!WARNING]
> CallbackMediator does NOT implement a unified event bus with named event types. It is a collection of bespoke `std::function` callbacks wired between specific components at startup. The table below documents the actual wiring connections established by `wireAll()`.

| Connection | Wired By (in wireAll) | Mechanism | Subscribers (target components) |
|-----------|----------------------|-----------|-------------------------------|
| **Map loaded** | `wireMapOperationCallbacks` | `MapOperationHandler::setMapLoadedCallback` (std::function) | Application::onMapLoaded, SearchController::onMapLoaded, WorkspaceController, hotkey wiring |
| **Map saved** | `wireMapOperationCallbacks` | `MapOperationHandler::setMapSavedCallback` (std::function) | (declared but NOT wired by CallbackMediator — set elsewhere) |
| **Tab changed** | `wireTabCallbacks` | `MapTabManager::setTabChangedCallback` (std::function) | WorkspaceController, WindowController, state synchronization |
| **Selection changed** | (Observer pattern) | `ISelectionObserver::onSelectionChanged` (virtual) | SelectionOverlay, BrowseTile, PropertyPanel |
| **Session modified** | `wireTabCallbacks` | `MapTabManager::setSessionModifiedCallback` (std::function) | Rendering cache invalidation |

> [!IMPORTANT]
> The following state changes have **NO callback/event** and are handled via **frame-by-frame polling**:
> - Floor changes — `ViewSettings::current_floor` is polled each frame by renderers
> - Brush changes — `BrushController` state is polled each frame by PreviewOverlay
> - View settings toggles — `ViewSettings` bools are polled each frame by render passes
> - Tile modifications — handled via `HistoryManager` undo/redo + render cache invalidation (no discrete tile-modified event)
> - Map close — uses `requestCloseTab` deferred action flow, not a publish/subscribe event

### 5.2. What It Is
`CallbackMediator` is **NOT** a publish/subscribe event bus. It is a **one-time wiring helper** that runs exactly once at startup (`Application::wireCallbacks()`) to connect inter-component callbacks via `std::function` assignments. After `wireAll()` completes, `CallbackMediator` is never used again — all connections persist on the target components.

### 5.3. Architecture and Context

```cpp
CallbackMediator::Context ctx;
ctx.tab_manager = &tab_manager_;
ctx.map_panel = map_panel_.get();
ctx.menu_bar = menu_bar_.get();
// ... fill all pointers
mediator.wireAll(ctx);
```

The `Context` structure contains non-owning pointers to all major components (Platform, Managers, Services, Rendering, UI, Controllers, and Dialogs) as well as raw `std::function` callbacks that connect back to `Application`.

`Application::wireCallbacks()` (in `Application.cpp`) creates the `Context`, fills all pointers, and calls `callback_mediator_.wireAll(ctx)` exactly once at startup.

> [!NOTE]
> The method that creates and calls `CallbackMediator` is `Application::wireCallbacks()`, not `initializeCallbacks()`.

### 5.4. Wiring Methods

`CallbackMediator` establishes direct connections using these dedicated internal methods:

| Method | What It Wires |
|--------|---------------|
| `wirePlatformCallbacks` | GLFW window close/resize events → Application |
| `wireTabCallbacks` | Tab switch actions → state synchronization |
| `wireMapOperationCallbacks` | File operations (open, save, import) → `MapOperationHandler` |
| `wireMenuCallbacks` | Menu bar options → dialog triggers and logic |
| `wireSecondaryClientCallbacks` | Actions triggering secondary client logic |
| `wireRibbonCallbacks` | Ribbon panel actions → services |
| `wireCleanupCallbacks` | Map cleanup triggers |
| `wireSearchCallbacks` | Search input → Search controllers/services |
| `wireInputCallbacks` | Input actions mapped from controllers |
| `wireMinimapCallbacks` | Minimap interactions |

### 5.5. When to Use

| Scenario | Solution |
|----------|----------|
| A component always calls another | Use constructor/context injection |
| A component acts as a generic UI element that fires a known action | Use `CallbackMediator` to map the `std::function` |
| Multiple systems need to react to a state change | Rely on frame-by-frame polling of `MapTabManager` or `AppStateManager` |

---

## 6. Rendering Pipeline

### 6.1. Per-Frame Sequence

```
Application::run() main loop
    │
    ├── 1. platform_manager_.update()          → glfwPollEvents()
    │
    ├── 2. update()                            → version_manager_.update()
    │                                              → sprite_manager_->processAsyncLoads()
    │
    └── 3. render()                            → render_orchestrator_.render(ctx)
            │
            ├── beginFrame()
            │     └── ImGui::NewFrame()
            │
            ├── Calculate animation ticks
            │
            ├── [If state == Startup]:
            │     └── StartupDialog
            │
            ├── [If state == Editor]:
            │     │
            │     ├── MainWindow::renderEditor()       (UI layer)
            │     │     ├── MenuBar
            │     │     ├── ImGui::DockSpaceOverViewport
            │     │     └── For each active tab:
            │     │           └── MapPanel::render()
            │     │                 ├── MapRenderer::render(session, camera)
            │     │                 │     ├── Visibility culling
            │     │                 │     ├── ChunkRenderingStrategy (cached/dynamic)
            │     │                 │     │     └── SpriteBatch (sorted by atlas page)
            │     │                 │     └── OverlayManager::render()  (all overlays)
            │     │                 └── MinimapRenderer::render()
            │     │
            │     ├── renderEditorState(ctx, session)
            │     │     └── Ribbon, BrowseTile, PaletteWindows, BrushSizePanel
            │     │
            │     └── renderDialogs(ctx)
            │           └── All modal/popup dialogs
            │
            ├── renderNotifications()             → ImGui toast notifications
            │
            └── endFrame()
                  └── ImGui::Render(), glClear, swap buffers
```

> [!NOTE]
> `MapRenderer::render()` and overlay rendering happen inside `MainWindow::renderEditor()` (the UI layer), NOT inside `renderEditorState()`. See `RenderOrchestrator.cpp` for the actual implementation.

### 6.2. RenderingManager and MapRenderer

`RenderingManager` owns the core rendering components and ensures clean lifecycle management, especially during destructive events like client version switches.

- **`MapRenderer`**: The single shared renderer instance.
- **`RenderState`**: A per-session data cache mapped by `SessionID`. Contains VBO cache, lighting state, and dirty chunk sets.

```cpp
auto* map_renderer = rendering_manager->getRenderer();
auto* state = rendering_manager->getRenderState(session->getID());
map_panel.render(session->getMap(), *state, map_renderer, anim_ticks);
```

### 6.3. Chunk Caching System

Each chunk maintains a cached VBO of pre-built sprite quads managed by the `ChunkRenderingStrategy`. 

**Cache invalidation triggers:**
- **Tile Edit**: Modifying tiles invalidates the affected chunk.
- **Sprite Loaded**: `RenderingManager::invalidateCache()` invalidates all chunks.
- **Floor Change**: Recalculates chunk visibility.

### 6.4. The Golden Rendering Rule

> [!CAUTION]
> **`ChunkRenderingStrategy` (VBO path) MUST be pixel-identical to dynamic rendering paths.**
>
> If you change how ANY tile element renders, you MUST update BOTH the cached and dynamic rendering paths. If they diverge, you'll see flickering or incorrect tiles when chunks transition between dirty and cached states.

### 6.5. SpriteBatch & Batching Strategy

Sprites are sorted by atlas page. All sprites on the same page are batched into a single draw call.

### 6.6. Atlas System (SpriteAtlas / AtlasManager)

The system packs sprites into large `TextureAtlas` pages (e.g., 2048x2048). Background threads decompress sprites, and `SpriteAsyncLoader::process()` uploads pixel data asynchronously via PixelBufferObjects (PBOs).

### 6.7. Overlay System

Owned by `OverlayManager`, overlays render auxiliary information on top of the map. They are directly instanced within the manager (e.g., `SelectionOverlay`, `SpawnLabelOverlay`, `GridOverlay`, `PreviewOverlay`, `StatusOverlay`) and rendered in sequence after the main map tiles.

### 6.8. Shader Inventory

| Shader | Files | Purpose |
|--------|-------|---------|
| Tile/Sprite | `tile_batch.vert` / `tile_batch.frag` | Main tile rendering (cached VBO path) |
| Sprite Batch | `sprite_batch.vert` / `sprite_batch.frag` | Dynamic sprite batching (ItemRenderer, CreatureRenderer, etc.) |
| Static Mesh | `static_mesh.vert` / `static_mesh.frag` | Static mesh rendering |
| Tooltip Bubble | `tooltip_bubble.vert` / `tooltip_bubble.frag` | Tooltip bubble overlay |
| Color Overlay | `color_overlay.vert` / `color_overlay.frag` | Tinted overlay rendering (selection, grid regions) |
| Light (embedded) | Inline in `Rendering/Light/LightOverlay.cpp` | Light compositing (not a standalone file) |

> [!NOTE]
> **Not standalone shader files:** Grid lines and selection highlights use `ImDrawList` (ImGui immediate-mode). Shade/Ghost floors use `sprite_batch` shader with tint parameters. Minimap is CPU-side pixel rendering displayed as an ImGui `Image` (no dedicated shader).

All standalone shaders live in `ImguiMapEditor/shaders/`. They are compiled and validated at startup — failures are fatal.

### 6.9. Performance Rules for the Render Loop

```
RENDER LOOP = NO-ALLOCATION ZONE

✅ DO:
├── Use pre-allocated vectors (clear + reuse, not recreate)
├── Use FrameDataCollector for per-frame temporary buffers
├── Use SpriteBatch for all sprite draws (never raw glDrawArrays)
├── Use GLHandle RAII wrappers for all OpenGL objects
└── Batch by atlas page to minimize texture binds

❌ DON'T:
├── Allocate with new/delete
├── Resize std::vector (pre-reserve)
├── Create/destroy GL objects per frame
├── Call glDrawArrays directly (use SpriteBatch)
└── Make redundant GL state changes (batch draw calls)
```

---

## 7. Brush System — Status & Boundaries

> [!WARNING]
> The brush system is a **proof of concept**. Most complex brush types are non-functional. Read this section before touching any brush code.

### 7.1. Architecture

```
BrushController (Brushes/BrushController.h — orchestrates brush operations)
    │
    ├── IBrush (Brushes/Core/IBrush.h — interface)
    │     └── draw(), undraw(), getName(), getType(), canDraw(), isDraggable(), etc.
    │
    ├── BrushSettingsService (brush size, shape config: square/circle/custom)
    │
    ├── BrushRegistry (Brushes/BrushRegistry.h — RAW brush cache)
    │
    ├── Behaviors/ (Brushes/Behaviors/)
    │     ├── ItemPlacement (how items are placed on tiles)
    │     └── WeightedSelection (random item selection from brush alternatives)
    │
    ├── Data/ (Brushes/Data/)
    │     ├── BorderBlock (border block data for GroundBrush)
    │     ├── WallNode (wall alignment nodes for WallBrush)
    │     └── DoodadAlternative (composite item alternatives for DoodadBrush)
    │
    └── Stroke tracking
          ├── beginStroke()
          ├── continueStroke(pos)
          └── endStroke()  → commits to HistoryManager
```

### 7.2. BrushController API

`BrushController` (located at `Brushes/BrushController.h`, NOT `Brushes/Core/`) orchestrates brush operations:

| Method | Purpose |
|--------|---------|
| `setBrush(IBrush*)` | Activate a brush type |
| `applyBrush(pos)` | Apply current brush at tile position |
| `beginStroke()` / `continueStroke(pos)` / `endStroke()` | Multi-tile stroke tracking |

`BrushRegistry` is at `Brushes/BrushRegistry.h` (NOT `Brushes/Core/`). It manages RAW brush instances and is used by `TilesetService`, `MapLoadingService`, and `TilesetXmlReader`.

### 7.3. Brush Types Implemented

| Brush | File | Status | Notes |
|-------|------|--------|-------|
| `RawBrush` | `Brushes/Types/RawBrush.h` | ✅ Functional | Places single items reliably |
| `EraserBrush` | `Brushes/Types/EraserBrush.h` | ✅ Functional | Removes items reliably |
| `CreatureBrush` | `Brushes/Types/CreatureBrush.h` | ✅ Functional | Places creatures |
| `SpawnBrush` | `Brushes/Types/SpawnBrush.h` | ✅ Functional | Creates spawn areas |
| `FlagBrush` | `Brushes/Types/FlagBrush.h` | ✅ Functional | Places map flags |
| `WaypointBrush` | `Brushes/Types/WaypointBrush.h` | ✅ Functional | Places waypoints |
| `HouseBrush` | `Brushes/Types/HouseBrush.h` | ✅ Functional | Assigns house zones |
| `PlaceholderBrush` | `Brushes/Types/PlaceholderBrush.h` | ⚠️ Stub | Stub for unimplemented brush types |

### 7.4. What's Broken & Why

| Missing Feature | Impact | Why It Matters |
|----------------|--------|----------------|
| **Auto-bordering** | Ground brushes don't calculate border transitions | In RME, this is the most complex brush feature — it automatically places transition tiles between terrain types (grass→dirt, sand→water, etc.) |
| **Auto-alignment** | Wall brushes don't auto-connect segments | Walls should auto-detect adjacent walls and select correct wall piece (corner, T-junction, straight) |
| **BrushRegistry population** | XML brush definitions parsed but not registered at runtime | Brushes defined in `brushes.xml` are read by `BrushXmlReader` (which contains TODO comments at lines 189, 223, 289, 311, 334) but the runtime `BrushRegistry` is not wired to them |
| **Complex brush types** | GroundBrush, WallBrush, DoodadBrush, CarpetBrush classes don't exist | `BrushXmlReader` parses all brush XML types but the C++ brush classes for these types are not implemented |

### 7.5. Safe to Build On vs Needs Rewriting

**✅ Safe foundations (use these):**
- `IBrush` interface (`Brushes/Core/IBrush.h`) — well-designed, stable
- `BrushController` (`Brushes/BrushController.h`) — stroke/apply lifecycle is sound
- `BrushSettingsService` — brush size/shape works (square, circle, custom shapes)
- `RawBrush`, `EraserBrush`, `CreatureBrush`, `SpawnBrush`, `FlagBrush`, `WaypointBrush`, `HouseBrush` — reliable implementations
- `ItemPlacement` / `WeightedSelection` (`Brushes/Behaviors/`) — handles item placement strategies and weighted random selection from brush alternatives
- `PaletteWindowManager` → `PaletteWindow` → `TilesetGridWidget` → `BrushController::setBrush()` — palette-to-brush wiring is **functional** for all implemented brush types

**❌ Needs significant work (don't build on these):**
- `GroundBrush` auto-bordering system — class doesn't exist, needs full implementation
- `WallBrush` auto-alignment system — class doesn't exist, needs full implementation
- `DoodadBrush` / `CarpetBrush` / `TableBrush` — classes don't exist, needs full implementation
- `BrushRegistry` ↔ XML wiring — `BrushXmlReader` parses definitions but TODOs block registration

> [!NOTE]
> The classes `IBrushBehavior`, `PointBehavior`, and `AreaBehavior` referenced in older documentation do **not** exist in the codebase. Brush behavior is handled by `BrushSettingsService::getBrushPositions()` (for tile position calculation) and `Brushes/Behaviors/ItemPlacement` + `WeightedSelection` (for item placement logic).

---

## 8. Stability Tiers — What to Touch, What to Avoid

### 8.1. 🟢 STABLE — Safe to Build On

| System | Files | Notes |
|--------|-------|-------|
| Domain layer | `Domain/` (Tile, Item, ChunkedMap, Position) | Core data model, well-tested |
| History system | `Domain/History/` | Undo/redo fully functional with LZ4 compression |
| Selection system | `Domain/Selection/`, `Services/Selection/` | Copy/paste/cut/delete work |
| Rendering pipeline | `Rendering/` (all passes, overlays) | Multi-pass pipeline is solid |
| Sprite loading | `Services/SpriteManager`, `Services/SpriteAsyncLoader` | Async pipeline robust |
| I/O reading | `IO/` (OTBM, OTB, DAT, SPR readers) | All readers functional for tested versions |
| Service architecture | `Services/`, `Application/` | DI wiring pattern mature |
| Controller pattern | `Controllers/` | Input handling, UI coordination works |
| Camera / navigation | `Rendering/Camera/ViewCamera` | Pan, zoom, floor switching |
| Light system | `Rendering/Light/` | Dynamic lighting fully functional |
| Grid / overlays | `Rendering/Overlays/` | All overlays functional |
| Creature simulation | `Services/CreatureSpriteService` | Outfit rendering and animation |
| Config persistence | `Services/ConfigService` | Settings save/load works |

### 8.2. 🟡 PARTIAL — Use with Caution

| System | Files | What Works | What Doesn't |
|--------|-------|-----------|--------------|
| Brush system | `Brushes/` | RawBrush, Eraser, Creature, Spawn, Flag, Waypoint, House | Auto-border, auto-align, complex brush types unimplemented |
| OTBM writing | `IO/Otbm/OtbmWriter` | Basic map save | Some attributes not serialized |
| House editing | `Domain/House`, UI dialogs | Houses load from OTBM | House editing UI is minimal |
| Spawn editing | `Domain/Spawn`, UI | Spawns display, basic editing | Radius visualization edge cases |
| Map merge | `Services/MapMergeService` | Basic merge works | Conflict resolution minimal; lives at `Services/` root, NOT `Services/Map/` |
| Map cleanup | `Services/Map/MapCleanupService` | Basic cleanup | Not all invalid states detected |

### 8.3. 🔴 NOT FUNCTIONAL — Do Not Build On

| System | Files | Status |
|--------|-------|--------|
| Auto-bordering | `Brushes/Types/GroundBrush` | **Does not exist.** `BrushXmlReader::parseGroundBrush()` parses XML but class is unimplemented (TODO at line 189) |
| Auto-alignment | `Brushes/Types/WallBrush` | **Does not exist.** `BrushXmlReader::parseWallBrush()` parses XML but class is unimplemented (TODO at line 223) |
| BrushRegistry ↔ XML wiring | `Brushes/BrushRegistry.h`, `IO/BrushXmlReader.cpp` | XML definitions parsed but ground/wall/doodad/carpet/table brushes are NOT registered (5 TODOs in BrushXmlReader.cpp) |
| Multi-tile Doodads | `Brushes/Types/DoodadBrush` | **Does not exist.** XML parsed but class unimplemented (TODO at line 289) |
| Carpet/Table Brushes | `Brushes/Types/CarpetBrush`, `TableBrush` | **Does not exist.** XML parsed but classes unimplemented (TODOs at lines 311, 334) |

---

## 9. Application Lifecycle

### 9.1. Startup Sequence

```
main()
  └── Application::initialize()
        ├── initializePlatform()          → GLFW, OpenGL context, ImGui, SettingsRegistry (services created here)
        ├── initializeLifecycleManagers() → SessionLifecycleManager, SessionWiringService
        ├── initializeUIComponents()      → UIFactory creates controllers/views
        ├── wireCallbacks()               → CallbackMediator::wireAll()
        ├── state_manager_.setStartupUpdater(...) / setEditorUpdater(...)
        │
        └── run()  → enters main loop (starts in State::Startup by default)
              │
              ├── [Startup state]
              │     ├── StartupDialog shown
              │     ├── User selects client version
              │     ├── ClientVersionManager loads DAT/SPR/OTB
              │     └── AppStateManager::transition(State::Editor) on map load
              │
              └── [Editor state]
                    └── Main loop renders normally
```

> [!NOTE]
> `Application::initializeServices()` is declared in `Application.h` but **not called**. Services are initialized inside `initializePlatform()` via `SettingsRegistry`. The `AppStateManager` starts in `State::Startup` by default (constructor), so no explicit `transition(Startup)` is needed at startup.

### 9.2. Main Loop

```cpp
void Application::run() {
    while (!should_quit_ && !platform_manager_.shouldClose()) {
        platform_manager_.update();       // → WindowController → glfwPollEvents
        
        // Frame-tick contracts (MUST happen every frame)
        update();                         // → version_manager_.update()
                                          //   → sprite_manager_->processAsyncLoads()
                                          //     → async_loader_->process()
        
        // Render everything
        render();                         // → render_orchestrator_.render(ctx)
        
        // Safe deferred cleanup
        processDeferredActions();         // Session destruction queue
    }
}
```

> [!IMPORTANT]
> `SpriteAsyncLoader::process()` is NOT called directly from `Application`. The call chain is: `Application::update()` → `ClientVersionManager::update()` → `SpriteManager::processAsyncLoads()` → `SpriteAsyncLoader::process()`. The documented `sprite_async_loader_` member does **not** exist on `Application`. The async loader is owned by `SpriteManager`.

### 9.3. Shutdown Sequence

```
Application::shutdown()
    ├── Save configuration (ConfigService)
    ├── Prompt save for modified sessions
    ├── Destroy all EditorSessions (release GPU resources)
    ├── Destroy services (release resources)
    ├── ImGui shutdown
    ├── OpenGL cleanup
    └── GLFW terminate
```

---

## 10. Layer Dependency Rules

### 10.1. Layer Stack

```
┌──────────────────────────────┐
│   Presentation (MainWindow)  │  ← Top: renders everything
├──────────────────────────────┤
│   UI (ImGui panels/widgets)  │
├──────────────────────────────┤
│   Controllers (input/coord)  │
├──────────────────────────────┤
│   Application (lifecycle)    │
├──────────────────────────────┤
│   Services (business logic)  │
├──────────────────────────────┤
│   Domain (data models)       │
├──────────────────────────────┤
│   Rendering (OpenGL/GPU)     │  ← Parallel to Services, below Application
├──────────────────────────────┤
│   I/O (file formats)        │
├──────────────────────────────┤
│   Platform (GLFW/OS)         │  ← Bottom: hardware abstraction
└──────────────────────────────┘
```

### 10.2. Allowed Dependencies

| Layer | Can Depend On |
|-------|--------------|
| Presentation | UI, Controllers, Application |
| UI | Controllers (via views), Services (direct queries) |
| Controllers | Services, Domain, CallbackMediator |
| Application | Services, Domain, Rendering, I/O, Platform |
| Services | Domain, I/O |
| Domain | Nothing (pure data) |
| Rendering | Domain (read-only), Services (SpriteManager) |
| I/O | Domain, Platform |
| Platform | Nothing |

### 10.3. Forbidden Dependencies

| Rule | Why |
|------|-----|
| **Rendering → UI** | Rendering must not know about ImGui panels |
| **Domain → anything** | Domain is pure data, no external deps |
| **Services → UI** | Services never call ImGui |
| **Services → Rendering** | Services don't issue GL calls |
| **Controllers → Controllers** | Controllers are independent; coordinate via CallbackMediator |
| **I/O → Services** | I/O reads/writes data, doesn't call business logic |
| **Any layer → Application.cpp** | Application.cpp is entry point, not a dependency |

> [!WARNING]
> **Known violations (documented for awareness):**
> - `Rendering/Frame/RenderOrchestrator.cpp` includes ~12 UI headers (orchestrates all UI rendering)
> - `Domain/MapInstance.h` depends on `Services/Selection/SelectionService.h`
> - `IO/Otbm/OtbmReader.cpp` and other I/O files depend on `Services/ClientDataService.h`
> - `Services/AppSettings.cpp` includes `UI/Core/Theme.h`
> - `Services/SpriteManager.h` depends on `Rendering/` for texture/atlas types
> These violations are acknowledged technical debt. New code should respect the layer rules.

---

## 11. Directory Ownership — When to Edit Which File

### 11.1. By Feature Type

| I want to... | Edit these files | Never edit |
|--------------|-----------------|------------|
| **Add a UI panel** | `UI/Panels/MyPanel.h`, `Controllers/MyController.h`, `Application/UIFactory.cpp`, `Presentation/MainWindow.cpp` | `Application.cpp` |
| **Add a service** | `Services/MyService.h/.cpp`, `Application.h` (add unique_ptr), `Application.cpp` (init only) | UI files |
| **Add a brush** | `Brushes/Types/MyBrush.h/.cpp`, `Brushes/BrushRegistry.h` (not `Brushes/Core/`) | Rendering files |
| **Add an overlay** | `Rendering/Overlays/MyOverlay.h/.cpp`, register in `OverlayManager` | UI files, Domain files |
| **Add a render pass** | `Rendering/Passes/MyPass.h/.cpp`, wire in `MapRenderer` | UI files, Service files |
| **Add a map operation** | `Services/Map/MyOpService.h/.cpp`, use HistoryManager pattern | Application.cpp |
| **Add a dialog** | `UI/Dialogs/MyDialog.h/.cpp`, `Presentation/Dialogs/MyDialogController.h` (if complex), `Application/DialogContainer.h` | Application.cpp |
| **Add a hotkey** | `Services/HotkeyRegistry` (register), `Controllers/HotkeyController` (handle) | Application.cpp |
| **Add a file format** | `IO/MyReader.h/.cpp`, wire into `MapLoadingService` or `MapSavingService` | UI files |
| **Modify tile rendering** | `Rendering/Tile/ItemRenderer`, `Rendering/Map/TileRenderer` | Update BOTH cached and dynamic paths! |
| **Add a preview provider** | `Services/Preview/MyPreviewProvider.h/.cpp`, register in `PreviewService` | Rendering files |

### 11.2. Full Directory Map

| Directory | Owns | Never Contains | When to Edit |
|-----------|------|----------------|-------------|
| `Application/` | Lifecycle, managers, factories, session | Direct UI rendering, business logic | Adding new managers, modifying startup, wiring new services |
| `Application/Coordination/` | VersionSwitchCoordinator | UI code | Modifying version switching logic |
| `Application/Selection/` | FloorScopeHelper, LassoSelectionProcessor, SmartSelectionStrategy | Rendering | Modifying selection state management |
| `Application/` (root) | SessionLifecycleManager, PersistenceManager, PlatformManager | — | Note: these managers live at `Application/` root, NOT `Application/Coordination/` |
| `Brushes/` | BrushRegistry, BrushController, BrushSystem | UI code | Brush orchestration (Registry and Controller are at `Brushes/` root, NOT `Brushes/Core/`) |
| `Brushes/Core/` | IBrush interface, BrushBase | UI, rendering | Extending brush API |
| `Brushes/Types/` | Concrete brush implementations | UI, rendering | Adding new brush types |
| `Brushes/Behaviors/` | ItemPlacement, WeightedSelection | UI, rendering | Adding item placement strategies |
| `Brushes/Data/` | BorderBlock, WallNode, DoodadAlternative | UI, rendering | Brush data structures (used by BrushXmlReader) |
| `Brushes/Enums/` | BrushEnums | UI, rendering | Brush enum types |
| `Controllers/` | Input handling, UI-service coordination | Direct rendering, domain logic | Adding input handlers, new controllers |
| `Core/` | Configuration constants, app-wide types | Business logic | Modifying config keys, constants |
| `Domain/` | Pure data models (Tile, Item, Creature, Position) | I/O, UI, rendering, services | Adding/modifying data structures |
| `Domain/History/` | Undo/redo system | UI code | Modifying undo behavior (rarely needed) |
| `Domain/Palette/` | Palette data structures | UI rendering | Adding palette types |
| `Domain/Selection/` | SelectionBucket, selection data | UI rendering | Modifying selection storage |
| `Domain/Search/` | ISearchProvider, MapSearchResult, SearchFilterTypes | UI | Search extensibility |
| `Domain/Tileset/` | Tileset data structures | UI rendering | Adding tileset types |
| `IO/` | File format readers/writers | Business logic, UI | Adding formats, fixing parsing bugs |
| `IO/Otbm/` | OTBM map format | UI, services | Fixing map load/save |
| `IO/Readers/` | DAT version-specific readers | UI, services | Supporting new client versions |
| `IO/Sec/` | SecItemParser, SecTileParser | UI, services | Secondary client data parsing |
| `IO/Flags/` | CanonicalFlags | UI, services | Flag type definitions |
| `Input/` | Low-level input abstraction | Business logic | Modifying raw input handling |
| `Platform/` | OS abstraction, GLFW wrappers | Business logic | Platform-specific fixes |
| `Presentation/` | MainWindow, MenuBar, top-level layout | Business logic | Modifying app layout, menu structure |
| `Presentation/Dialogs/` | Dialog controller classes | Rendering | Adding dialog logic |
| `Rendering/` | OpenGL pipeline, GPU resources | Application deps, direct domain mutation | Adding visual features |
| `Rendering/Animation/` | Sprite animation timing | Domain | Modifying animation behavior |
| `Rendering/Backend/` | SpriteBatch, RingBuffer, MultiDrawIndirect | Domain, UI | Low-level GPU optimizations |
| `Rendering/Camera/` | ViewCamera | Domain | Camera behavior changes |
| `Rendering/Core/` | Framebuffer, Shader, Texture, VAO wrappers | Domain, UI | GPU resource management |
| `Rendering/Frame/` | RenderOrchestrator, RenderingManager, RenderState | UI | Frame-level orchestration changes |
| `Rendering/Light/` | LightManager, LightGatherer, LightOverlay | UI | Lighting system changes |
| `Rendering/Map/` | MapRenderer, TileRenderer | UI | Map rendering changes |
| `Rendering/Minimap/` | MinimapRenderer, MinimapTexture | UI | Minimap rendering |
| `Rendering/Overlays/` | Grid, Selection, Preview, Tooltip overlays | Domain | Adding/modifying overlays |
| `Rendering/Passes/` | LightingPass, GhostFloor, SpawnTint | UI | Adding render passes |
| `Rendering/Resources/` | AtlasManager, ShaderLoader, TextureAtlas, SpriteAtlasLUT | UI | GPU resource management (note: `PixelBufferObject` is in `Rendering/Core/`) |
| `Rendering/Selection/` | ISelectionDataProvider, SelectionDataProviderAdapter | UI | Selection data provider interface |
| `Rendering/Tile/` | ItemRenderer, CreatureRenderer, GroundRenderer, ChunkRenderingStrategy | UI | Tile rendering logic |
| `Rendering/Utils/` | CoordUtils, MathUtils, SpriteEmitter | Domain | Utility changes |
| `Rendering/Visibility/` | ChunkVisibilityManager, FloorIterator | UI | Culling logic |
| `Services/` | Business logic, data access | UI code | Adding/modifying services |
| `Services/Brushes/` | Border, Wall, Carpet, Table lookup | UI | Brush lookup logic |
| `Services/Map/` | Loading, Saving, Editing, Search, Cleanup | UI | Map operations (note: `MapMergeService` lives at `Services/` root, NOT `Services/Map/`) |
| `Services/Preview/` | Preview providers | UI rendering | Adding preview types |
| `Services/Selection/` | Selection service | UI rendering | Selection operations |
| `UI/` | ImGui rendering, visual state | Business logic, rendering | UI changes |
| `UI/Core/` | Theme system | Rendering | Theme modifications |
| `UI/Dialogs/` | Dialog views (ImGui) | Business logic | Dialog UI changes |
| `UI/DTOs/` | Data transfer objects for UI | Rendering | Adding UI data structures |
| `UI/Map/` | Map panel | Business logic (use controller) | Map viewport UI |
| `UI/Panels/` | Dockable panels | Business logic | Panel UI changes |
| `UI/Ribbon/` | Ribbon toolbar | Business logic | Ribbon UI changes |
| `UI/Utils/` | UI utility functions | Business logic | Adding UI helpers |
| `UI/Widgets/` | Reusable widgets | Business logic | Adding/modifying widgets |
| `UI/Windows/` | Floating windows | Business logic | Window UI changes |
| `Utils/` | Pure utility functions | State, business logic | General utility changes |
| `shaders/` | GLSL shader source files | C++ code | Shader modifications |

---

## 12. History System (Undo/Redo)

### 12.1. Architecture

```
HistoryManager (per-session, main API)
    │
    ├── HistoryBuffer (ring buffer, configurable depth)
    │       └── HistoryEntry (one undoable operation)
    │               ├── beforeSnapshots_: vector<TileSnapshot>
    │               ├── afterSnapshots_: vector<TileSnapshot>
    │               ├── description_: string
    │               └── type_: ActionType
    │
    └── TileSnapshot (serialized tile state)
            ├── position_: Position
            └── data_: vector<uint8_t> (LZ4 compressed)
```

| File | Purpose |
|------|---------|
| `Domain/History/HistoryManager.h` | Main API: beginOperation, recordTileBefore, endOperation, undo, redo |
| `Domain/History/HistoryEntry.h` | Single undoable operation with before/after snapshots |
| `Domain/History/TileSnapshot.h` | Serialized tile state |
| `Domain/History/TileSnapshotCodec.h` | LZ4 serialization/compression |
| `Domain/History/HistoryBuffer.h` | Ring buffer storage (Boost.CircularBuffer) |

### 12.2. Recording an Undoable Operation

```cpp
void MyService::modifyTiles(EditorSession& session, const std::vector<Position>& positions) {
    auto* history = session.getHistoryManager();
    auto* map = session.getMap();
    
    // 1. BEGIN — declares operation name and type
    history->beginOperation("My Operation", ActionType::Other);
    
    // 2. RECORD BEFORE — snapshot every tile you will modify
    for (const auto& pos : positions) {
        history->recordTileBefore(pos, map->getTile(pos));
    }
    
    // 3. MODIFY — make your changes
    for (const auto& pos : positions) {
        map->setTile(pos, newTile);
    }
    
    // 4. END — automatically captures AFTER states
    history->endOperation(map);
}
```

**ActionTypes:**
```cpp
enum class ActionType {
    Draw,       // Brush painting
    Move,       // Moving selection
    Paste,      // Pasting clipboard
    Delete,     // Deleting items
    Reorder,    // Reordering items in tile
    Properties, // Changing tile/item properties
    Spawn,      // Spawn/creature changes
    Other       // Catch-all
};
```

> [!TIP]
> Always call `recordTileBefore()` for EVERY tile you intend to modify, even if you're not sure the modification will happen. The system handles deduplication.

---

## 13. I/O Layer

### 13.1. Binary Formats

| Reader/Writer | Format | Purpose | Status |
|---------------|--------|---------|--------|
| `OtbmReader` | `.otbm` | Open Tibia Binary Map (read) | ✅ Stable |
| `OtbmWriter` | `.otbm` | Open Tibia Binary Map (write) | ⚠️ Partial (some attributes missing) |
| `OtbReader` | `.otb` | Item type definitions | ✅ Stable |
| `SprReader` | `.spr` | Sprite graphics data | ✅ Stable |
| `DatReaderV*` | `.dat` | Item/creature metadata | ✅ Stable (all versions) |
| `SecReader` | `.sec` | Secondary client data | ✅ Stable |
| `NodeFileReader` | Binary nodes | Hierarchical binary format parser | ✅ Stable |

### 13.2. XML Formats

| Reader/Writer | Format | Purpose |
|---------------|--------|---------|
| `BrushXmlReader` | `brushes.xml` | Brush definitions |
| `CreatureXmlReader` | `creatures.xml` | Creature types |
| `ItemXmlReader` | `items.xml` | Item metadata |
| `TilesetXmlReader/Writer` | `tilesets.xml` | Tileset organization |
| `PaletteXmlReader` | `palettes.xml` | Palette definitions |
| `HouseXmlReader/Writer` | `houses.xml` | House data |
| `SpawnXmlReader/Writer` | `spawns.xml` | Spawn data |
| `MaterialsXmlReader` | `materials.xml` | Material definitions |

### 13.3. DAT Reader Hierarchy

```
DatReaderBase (Abstract)
  ├── DatReaderV710    (Tibia 7.10)
  ├── DatReaderV740    (Tibia 7.40)
  ├── DatReaderV755    (Tibia 7.55)
  ├── DatReaderV780    (Tibia 7.80)
  ├── DatReaderV860    (Tibia 8.60)
  └── DatReaderV1010   (Tibia 10.10+)
```

Version selection happens in `ClientVersionManager`. The correct reader is instantiated based on the client version selected by the user.

---

## 14. Service Wiring Checklist

### 14.1. Creating a New Service

```
☐ 1. Create Services/MyService.h and .cpp
☐ 2. Constructor takes dependencies as raw pointers
☐ 3. Add std::unique_ptr<Services::MyService> in Application.h
☐ 4. Initialize in Application::initializeServices()
☐ 5. If UI needs it: add to UIFactoryContext struct
☐ 6. If UI needs it: populate in Application::initializeUIComponents()
☐ 7. Pass to controllers/views via constructor injection
```

### 14.2. Creating a New Controller

```
☐ 1. Create Controllers/MyController.h and .cpp
☐ 2. Constructor receives services as raw pointers
☐ 3. Add factory method in UIFactory: createMyController(ctx)
☐ 4. Create instance in Application::initializeUIComponents()
☐ 5. Pass to the View that will use it
☐ 6. Controller exposes action methods (for View to call)
☐ 7. Controller exposes state queries (for View to render)
```

### 14.3. Creating a New UI Panel

```
☐ 1. Create UI/Panels/MyPanel.h (ImGui code only)
☐ 2. Create Controllers/MyController.h if complex logic needed
☐ 3. Add UIFactory::createMyPanel(ctx) method
☐ 4. Add to MainWindow::render() to draw the panel
☐ 5. Panel calls controller methods for actions
☐ 6. Panel queries controller for state to display
☐ 7. NEVER put business logic in the panel
```

### 14.4. Creating a New Overlay

```
☐ 1. Create Rendering/Overlays/MyOverlay.h implementing IOverlayRenderer
☐ 2. Implement render(), isVisible(), getRenderOrder()
☐ 3. Register in OverlayManager (during MapRenderer/RenderingManager init)
☐ 4. Add visibility toggle to ViewSettings (if user-controllable)
☐ 5. If data needed: collect via OverlayCollector in the frame
☐ 6. Use SpriteBatch or direct GL for drawing
```

### 14.5. Creating a New Dialog

```
☐ 1. Create UI/Dialogs/MyDialog.h (ImGui view)
☐ 2. If complex: create Presentation/Dialogs/MyDialogController.h
☐ 3. Add std::unique_ptr<MyDialog> to DialogContainer
☐ 4. Add open/show method to dialog
☐ 5. Wire open trigger in MenuBar or RibbonController
☐ 6. MainWindow or RenderOrchestrator calls dialog->render() each frame
```

---

## Appendix A: Managers Reference

| Manager | Location | Purpose | Scope |
|---------|----------|---------|-------|
| `MapTabManager` | `Application/` | Multiple open map sessions | App |
| `AppStateManager` | `Application/` | App state FSM (startup→running→shutdown) | App |
| `ClientVersionManager` | `Application/` | Client version detection/switching | App |
| `SessionLifecycleManager` | `Application/` | Session create/destroy | App |
| `PersistenceManager` | `Application/` | Config save/load | App |
| `PlatformManager` | `Application/` | Platform-specific init | App |
| `HistoryManager` | `Domain/History/` | Undo/redo for one map | Session |
| `RenderingManager` | `Rendering/Frame/` | MapRenderer lifecycle, RenderState management | App |
| `LightManager` | `Rendering/Light/` | Dynamic lighting | Rendering |
| `OverlayManager` | `Rendering/Overlays/` | Overlay registration/rendering | Rendering |
| `AtlasManager` | `Rendering/Resources/` | Sprite atlas page management | App |
| `ChunkVisibilityManager` | `Rendering/Visibility/` | Visible chunk calculation | Rendering |
| `SpriteManager` | `Services/` | Sprite loading, caching, atlas regions | App |

## Appendix B: Controllers Reference

| Controller | Location | Purpose |
|------------|----------|---------|
| `MapInputController` | `Controllers/` | Mouse/keyboard on map viewport |
| `HotkeyController` | `Controllers/` | Global keyboard shortcuts |
| `StartupController` | `Controllers/` | Startup flow (version selection, map loading) |
| `WorkspaceController` | `Controllers/` | Panel visibility and layout |
| `WindowController` | `Controllers/` | Application window state |
| `SearchController` | `Controllers/` | Search functionality |
| `SimulationController` | `Controllers/` | Creature animation mode |
| `BrushController` | `Brushes/` | Brush selection/application/stroke tracking |
| `RibbonController` | `UI/Ribbon/` | Ribbon bar state and actions |
| `CleanupController` | `Presentation/Dialogs/` | Map cleanup dialog logic |
| `ImportMapController` | `Presentation/Dialogs/` | Map import dialog logic |
| `TownPickController` | `Presentation/Dialogs/` | Town selection dialog logic |

## Appendix C: Utility Index (Reuse These!)

### UI Utilities

| Utility | Location | Use For |
|---------|----------|---------|
| `PreviewUtils` | `UI/Utils/PreviewUtils.hpp` | Item/creature preview images |
| `UIUtils` | `UI/Utils/UIUtils.hpp` | Tooltips, common ImGui patterns |
| `PropertyWidgets` | `UI/Widgets/Properties/PropertyWidgets.cpp` | Property editors |
| `ConfirmationDialog` | `UI/Dialogs/ConfirmationDialog.cpp` | Yes/No prompts |
| `UnsavedChangesModal` | `UI/Dialogs/UnsavedChangesModal.cpp` | Save-before-close prompts |
| `Theme.h` | `UI/Core/Theme.h` | ImGui color constants |
| `RibbonUtils` | `UI/Ribbon/Utils/RibbonUtils.h` | Ribbon button helpers |
| `NotificationHelper` | `Presentation/NotificationHelper.h` | Toast notifications |

### Rendering Utilities

| Utility | Location | Use For |
|---------|----------|---------|
| `CoordUtils` | `Rendering/Utils/CoordUtils.h` | World↔Screen↔Tile coordinate conversions |
| `SpriteEmitter` | `Rendering/Utils/SpriteEmitter.h` | Batched sprite draws |
| `MathUtils` | `Rendering/Utils/MathUtils.h` | Math helpers |
| `SpriteBatch` | `Rendering/Backend/SpriteBatch.cpp` | Batched rendering |
| `FloorIterator` | `Rendering/Visibility/FloorIterator.h` | Iterate visible floors |
| `ColorFilter` | `Rendering/ColorFilter.h` | Color transformations |
| `FrameDataCollector` | `Rendering/Frame/FrameDataCollector.h` | Per-frame temporary buffers (no-alloc) |
| `OverlayCollector` | `Rendering/Overlays/OverlayCollector.h` | Collect overlay data per frame |

### Domain Utilities

| Utility | Location | Use For |
|---------|----------|---------|
| `Position` | `Domain/Position.h` | 3D tile coords + operators + hash |
| `HistoryManager` | `Domain/History/HistoryManager.h` | Undo/redo API |
| `TileSnapshot` | `Domain/History/TileSnapshot.h` | Tile state capture/restore |
| `SelectionBucket` | `Domain/Selection/SelectionBucket.h` | Selected items container |

### I/O Utilities

| Utility | Location | Use For |
|---------|----------|---------|
| `XmlUtils` | `IO/XmlUtils.cpp` | XML parsing helpers (pugixml wrappers) |
| `BinaryReader` | `IO/BinaryReader.cpp` | Binary file reading |

### General Utilities

| Utility | Location | Use For |
|---------|----------|---------|
| `SpriteUtils` | `Utils/SpriteUtils.cpp` | Sprite index calculation |
| `ScopedFlag` | `Utils/ScopedFlag.h` | RAII bool flag setter |
| `ImageBlending` | `Utils/ImageBlending.h` | Image alpha blending |

---

## Appendix D: Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes | PascalCase | `ChunkedMap`, `TileRenderer` |
| Functions | camelCase | `loadMap()`, `getTile()` |
| Member variables | snake_case + trailing `_` | `sprite_manager_`, `is_modified_` |
| Constants | UPPER_SNAKE_CASE | `CHUNK_SIZE`, `MAX_FLOORS`, `TILE_PIXEL_SIZE` |
| Namespaces | PascalCase | `MapEditor::Domain` |
| Files | PascalCase | `ChunkedMap.cpp`, `ChunkedMap.h` |
| Enums | PascalCase type, PascalCase values | `ActionType::Draw` |

---

## Appendix E: Pre-Implementation Checklist

Before writing ANY code, verify:

```
☐ Does this functionality already exist? (grep the codebase)
☐ Is there a utility I should reuse? (check Appendix C)
☐ Which directory SHOULD own this logic? (check Section 11)
☐ Am I about to add logic to Application.cpp? → STOP
☐ Am I about to call gl* from a background thread? → STOP
☐ Am I building on a 🔴 NOT FUNCTIONAL system? → STOP and discuss
☐ Do I need undo/redo? → Use HistoryManager pattern (Section 12)
☐ Does this affect tile rendering? → Update BOTH cached and dynamic paths (Section 6.3)
☐ Am I creating a new async system? → Follow process() contract (Section 2.5)
☐ Does this need to notify multiple components? → Use CallbackMediator (Section 5)
☐ Am I about to duplicate code? → Create shared utility instead
```

**The mantra:** `SEARCH → REUSE → ORGANIZE → IMPLEMENT`


</architecture>