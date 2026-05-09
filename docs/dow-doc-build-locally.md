# Build Locally Guide

This guide explains how to build Card Collection Manager 3 on Windows and Linux, how dependency management works, and how to run tests locally.

## Who this is for

Use this guide if you want to:

- build the app from source
- understand how dependencies are resolved
- choose between bundled vs system wxWidgets
- troubleshoot local build issues

## Project build model

The project uses CMake and builds a native desktop executable:

- executable target: `ccm` (output binary name: `ccm3` / `ccm3.exe`)
- language standard: C++20
- layering:
  - `ccm_core` (pure logic)
  - `ccm_ui_wx` (wxWidgets UI)
  - `ccm` (composition root)

## Dependency management

Dependencies are managed through CMake `FetchContent` (see `cmake/Dependencies.cmake`).

Pinned versions:

- `nlohmann/json` `v3.11.3`
- `libcpr/cpr` `1.10.5`
- `wxWidgets` `v3.2.5`
- `doctest` `v2.4.11` (only when tests are enabled)

### How it works

- On first configure, CMake downloads and configures upstream sources.
- On first full build, heavy dependencies (notably wxWidgets/curl) are built locally.
- Subsequent builds reuse cached dependencies in `build/_deps`.

### Using system wxWidgets

By default, wxWidgets is fetched/built from source.  
For faster local iteration you can use an installed wxWidgets:

- set `-DCCM_USE_SYSTEM_WX=ON`

## Prerequisites

## Windows

### Recommended path: Clang + Ninja

- LLVM/Clang 14+ in `PATH`
- CMake 3.22+
- Ninja in `PATH`

### Verified fallback path: MSYS2 UCRT64 + MinGW-w64 GCC

- MSYS2 UCRT64 toolchain (`gcc`, `cmake`, `make`/`ninja`)
- CMake 3.22+

> MSVC is intentionally not supported.

## Linux

- CMake 3.22+
- C++ compiler (Clang or GCC with C++20 support)
- Ninja recommended
- system packages required when using system wx (for example GTK/wx dev packages)

## Build commands

Run all commands from repository root.

## Windows (Clang + Ninja, recommended)

```powershell
cmake -S . -B build -G Ninja
cmake --build build --parallel
.\build\bin\ccm3.exe
```

## Windows (MinGW Makefiles fallback)

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
.\build\bin\ccm3.exe
```

## Linux (Ninja)

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel
./build/bin/ccm3
```

## Build options

| Option | Default | Purpose |
|---|---|---|
| `CCM_USE_SYSTEM_WX` | `OFF` | Use installed wxWidgets instead of FetchContent wx build |
| `CCM_BUILD_TESTS` | `ON` | Build `ccm_core_tests` |
| `CMAKE_BUILD_TYPE` | `Release` | Standard CMake build type |
| `CCM_APP_VERSION` | `${PROJECT_VERSION} (localbuild)` | Embedded version string shown in app About dialog |

Example (faster local iteration):

```bash
cmake -S . -B build -DCCM_USE_SYSTEM_WX=ON -DCCM_BUILD_TESTS=OFF
cmake --build build
```

## Running tests locally

```bash
cmake -S . -B build -DCCM_BUILD_TESTS=ON
cmake --build build --target ccm_core_tests
ctest --test-dir build --output-on-failure
```

The automated test surface is concentrated in `core/` and infra adapters; UI tests are currently out of scope.

## Runtime notes

## Windows runtime DLLs

`cpr` is built shared, so `build/bin` contains runtime DLLs (for example `libcpr.dll`, `libcurl.dll`, `libzlib.dll`) next to `ccm3.exe`.

For MinGW/MSYS2 builds you also need UCRT runtime DLLs available (typically from MSYS2 UCRT64 `bin` on `PATH`).

## Common troubleshooting

- **First build is slow**
  - Expected. Fetch/build of wx/curl can take several minutes.

- **`Permission denied` while linking `ccm3.exe`**
  - App is still running. Close it, then rebuild.

- **Generator mismatch (Ninja vs MinGW Makefiles)**
  - Reuse the existing generator in that build directory, or create a new build directory.

- **Windows CI/local shell mismatch**
  - In CI with `shell: msys2 {0}`, ensure MSYS2 setup step runs before shell commands.

## Related docs

- `ci-cd-guide.md` — CI/CD behavior and release automation
- `versioning.md` — versioning policy and tag rules
- `adding-a-new-game.md` — adding support for a new TCG
