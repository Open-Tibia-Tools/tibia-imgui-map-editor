# Build Instructions

This project supports two package managers: **vcpkg** (default) and **Conan**.

## Prerequisites

- CMake 3.25+
- C++20 compatible compiler (MSVC 2022, GCC 11+, Clang 14+)
- Git

---

## Option 1: Build with vcpkg (Default)

vcpkg is automatically integrated via CMake's manifest mode.

### Windows (Visual Studio)

```powershell
# Clone the repository
git clone https://github.com/your-repo/Imgui-MapEditor.git
cd Imgui-MapEditor/ImguiMapEditor

# Configure (vcpkg will auto-install dependencies)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

### Linux/macOS

```bash
# Ensure vcpkg is installed and VCPKG_ROOT is set
export VCPKG_ROOT=/path/to/vcpkg

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

---

## Option 2: Build with Conan

### Install Conan

```bash
pip install conan
conan profile detect  # First time only
```

### Build

```powershell
cd ImguiMapEditor

# Install dependencies (generates conan_toolchain.cmake)
conan install . --output-folder=build --build=missing -s build_type=Release

# Configure with Conan toolchain
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

### Conan Profiles (Optional)

For Debug builds:
```bash
conan install . --output-folder=build --build=missing -s build_type=Debug
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| GLFW | Window/input handling |
| GLAD | OpenGL loader |
| GLM | Math library |
| spdlog | Logging |
| nlohmann_json | JSON parsing |
| pugixml | XML parsing |
| nativefiledialog-extended | Native file dialogs |
| LZ4 | Fast compression |
| Boost (iostreams, filesystem) | I/O utilities |
| stb | Image loading |
| fmt | String formatting |

---

## Output

The executable is created at:
- **Windows**: `build/Release/TibiaMapEditor.exe`
- **Linux/macOS**: `build/TibiaMapEditor`
