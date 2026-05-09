#documentation #build #cmake

# Build Locally Guide

This guide explains how to build Card Collection Manager 3 on Windows and Linux, how dependencies are resolved, and how to run tests locally. For architecture orientation, see [Intro For New Developers](intro-to-new-developers.md).

**Quick Setup:** run CMake configure, build, launch `ccm3`, then run `ctest` from the same build directory.

## Build Model

The project uses CMake and builds one desktop executable:

- executable target: `ccm` (output binary: `ccm3` / `ccm3.exe`)
- language standard: C++20
- layered targets: `ccm_core` (logic), `ccm_ui_wx` (wx UI), `ccm` (composition root)

## Dependency Model

Dependencies are managed with CMake `FetchContent` in `cmake/Dependencies.cmake`.

Pinned versions:

- `nlohmann/json` `v3.11.3`
- `libcpr/cpr` `1.10.5`
- `wxWidgets` `v3.2.5`
- `doctest` `v2.4.11` (only when tests are enabled)

On first configure, CMake downloads sources. On first full build, heavy dependencies (especially wxWidgets and curl) build locally. Later builds reuse cached dependencies under `build/_deps`.

## Use System wxWidgets

By default, the build fetches wxWidgets. For faster local iteration with an installed wxWidgets, set `-DCCM_USE_SYSTEM_WX=ON`.

## Prerequisites

### Windows

Recommended: Clang + Ninja

- LLVM/Clang 14+ on `PATH`
- CMake 3.22+
- Ninja on `PATH`

Verified fallback: MSYS2 UCRT64 + MinGW-w64 GCC

- MSYS2 UCRT64 toolchain (`gcc`, `cmake`, `make` or `ninja`)
- CMake 3.22+

MSVC is intentionally not supported.

### Linux

- CMake 3.22+
- Clang or GCC with C++20 support
- Ninja recommended
- required system packages when using system wxWidgets (for example GTK/wx dev packages)

## Build Commands

Run from repository root.

### Windows (Clang + Ninja)

```powershell
cmake -S . -B build -G Ninja
cmake --build build --parallel
.\build\bin\ccm3.exe
```

### Windows (MinGW Makefiles)

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
.\build\bin\ccm3.exe
```

### Linux (Ninja)

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel
./build/bin/ccm3
```

## Build Options

- `CCM_USE_SYSTEM_WX` (default `OFF`): use installed wxWidgets instead of fetched wxWidgets.
- `CCM_BUILD_TESTS` (default `ON`): build `ccm_core_tests`.
- `CMAKE_BUILD_TYPE` (commonly `Release`): standard CMake build type.
- `CCM_APP_VERSION` (default `${PROJECT_VERSION} (localbuild)`): app version string shown in About dialog.

Example for faster local iteration:
```bash
cmake -S . -B build -DCCM_USE_SYSTEM_WX=ON -DCCM_BUILD_TESTS=OFF
cmake --build build
```

## Run Tests Locally

From repository root:
```bash
cmake -S . -B build -DCCM_BUILD_TESTS=ON
cmake --build build --target ccm_core_tests
ctest --test-dir build --output-on-failure
```

Automated tests primarily cover `core/` and infrastructure adapters. UI testing is currently manual.

## Runtime Notes

### Windows Runtime DLLs

`cpr` builds as shared, so `build/bin` contains runtime DLLs (for example `libcpr.dll`, `libcurl.dll`, `libzlib.dll`) next to `ccm3.exe`.

For MinGW/MSYS2 builds, UCRT runtime DLLs must be available (typically via MSYS2 UCRT64 `bin` on `PATH`).

## Troubleshooting

- **First build is slow:** expected on cold dependency fetch/build, especially wxWidgets and curl.
- **`Permission denied` while linking `ccm3.exe`:** the app is still running; close it and rebuild.
- **Generator mismatch:** reuse the same generator for a build directory or create a new build directory.
- **Windows CI/local shell mismatch:** in CI jobs using `shell: msys2 {0}`, ensure MSYS2 setup runs before shell commands.

## Related Docs

- [CI/CD Guide](ci-cd-guide.md)
- [Versioning Guide](versioning.md)
- [Adding a new game to Card Collection Manager](adding-a-new-game.md)
