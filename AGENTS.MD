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