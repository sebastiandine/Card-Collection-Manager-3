#documentation #installer #nsis #windows

# Windows Installer Guide

This guide explains how the Windows installer for Card Collection Manager 3 is built, how it is configured, and how it cooperates with the rest of the build/release pipeline. For CI flow and artifact naming, see [CI/CD Guide](ci-cd-guide.md). For where the version string comes from, see [Versioning Guide](versioning.md).

**Quick Setup:** install NSIS (`makensis` on `PATH`), build the app into `build/bin/`, then run `makensis -DAPP_VERSION="<version>" scripts/installer.nsi` from the repository root.

## Installer Model

The installer is a single NSIS script: `scripts/installer.nsi`. It produces one self-contained executable that ships the entire `build/bin/` directory plus an embedded uninstaller.

Key facts:

- installer technology: NSIS (`makensis`) with the Modern UI 2 (MUI2) library
- output file: `ccm3-windows-installer.exe`, written to the repository root (resolved as `..\ccm3-windows-installer.exe` from `scripts/`)
- payload source: every file under `build/bin/` (resolved as `..\build\bin\*.*` from `scripts/`)
- icon: `scripts/installer_icon.ico` (resolved relative to `scripts/`)
- default install location: `%PROGRAMFILES64%\Card Collection Manager 3`
- elevation: `RequestExecutionLevel admin`

Because the installer pulls from `build/bin/` directly, it must run **after** a successful release build that has been bundled with all required runtime DLLs (see [Build Locally Guide](dow-doc-build-locally.md) for what ends up in `build/bin/`).

## Configuration Inputs

Almost everything the installer needs is hard-coded in `scripts/installer.nsi`. The only configurable input is the version string, supplied at `makensis` time:

- `APP_VERSION` — passed via `-DAPP_VERSION="<value>"`. Falls back to `"localbuild"` if not provided, so manual local runs still work.

This single value is reused in three visible places, so the installer, the uninstaller, and the OS Programs and Features entry all advertise the same version:

- installer/uninstaller window title (NSIS `Name`): `Card Collection Manager 3 <version>`
- footer / branding text on every wizard page (NSIS `BrandingText`): `Card Collection Manager 3 <version>`
- Add/Remove Programs `DisplayVersion` registry value, so Windows shows the version in its own column

The MUI welcome page and the uninstall confirm page reference `$(^Name)` internally, so embedding the version into `Name` is enough to make those pages say "Welcome to the Card Collection Manager 3 \<version\> Setup Wizard" and "Card Collection Manager 3 \<version\> will be uninstalled..." respectively, without any extra wiring.

## Installed Sections

The installer presents three sections on the Components page:

- **Core files (required)** — read-only (`SectionIn RO`). Copies the full `build/bin/` payload into `$INSTDIR`, writes the uninstaller, and registers the Add/Remove Programs entry plus an `App Paths` entry so `ccm3` resolves from `Win+R`.
- **Start Menu shortcuts** — creates a shortcut at the top level of the Start Menu plus a `Card Collection Manager 3` folder containing both the app shortcut and an "Uninstall" shortcut. Uses `SetShellVarContext all` so shortcuts go to the all-users Start Menu.
- **Desktop shortcut** — creates a desktop shortcut for all users.

Shortcut names intentionally do **not** include the version, so installing a newer version overwrites the existing shortcuts cleanly instead of leaving orphaned per-version entries behind.

## Registry Layout

The installer writes two registry roots, both under `HKLM` so an uninstall removes them cleanly regardless of which user launches it:

- `Software\Microsoft\Windows\CurrentVersion\Uninstall\Card Collection Manager 3` (the Add/Remove Programs entry):
    - `DisplayName` — product name
    - `DisplayVersion` — value of `APP_VERSION`
    - `DisplayIcon` — path to `ccm3.exe`
    - `UninstallString` / `QuietUninstallString` — interactive and silent uninstall commands
    - `InstallLocation` — `$INSTDIR`
    - `NoModify` / `NoRepair` — both `1` (we do not implement modify/repair flows)
- `Software\Microsoft\Windows\CurrentVersion\App Paths\ccm3.exe`:
    - default value — full path to `ccm3.exe`
    - `Path` — `$INSTDIR` so child processes inherit DLL search rights

The uninstall section (`Section "Uninstall"`) deletes both roots and removes the install directory and all created shortcuts. It uses `RMDir /r "$INSTDIR"` because the install dir is owned by the app.

## Build Commands

Run from the repository root.

### Local manual build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
makensis -DAPP_VERSION="0.1.0-localbuild" scripts/installer.nsi
```

The output is `ccm3-windows-installer.exe` at the repository root. With no `-DAPP_VERSION`, the installer self-labels as `localbuild` instead.

### CI build

Both Windows workflows install NSIS via MSYS2 (`mingw-w64-ucrt-x86_64-nsis`) and invoke the same script:

```yaml
- name: Build Windows installer
  run: makensis -DAPP_VERSION="${VERSION}" scripts/installer.nsi
```

The `${VERSION}` value comes from:

- `scripts/compute_master_semver.sh` for merged `master` PRs (semantic version, e.g. `1.2.3`)
- `scripts/compute_feature_version.sh` for non-`master` branches (e.g. `feature-dark-mode-a1b2c3d`)

The exact same `${VERSION}` is also passed to `cmake -DCCM_APP_VERSION=...`, so the installer/uninstaller, the Programs and Features entry, and the running app's About dialog always agree.

## Artifact Names

The CI artifacts produced from a single installer build are documented in [CI/CD Guide](ci-cd-guide.md), but for reference:

- raw build output: `ccm3-windows-installer.exe` (in repo root, regardless of version)
- feature workflow artifact: `ccm3-windows-installer-<version>` (folder containing the exe)
- master release asset: `ccm3-windows-installer-<semver>.exe` (renamed at release-asset packaging time)

Renaming happens in the workflow's release-assets step, not in `installer.nsi`, so the script's `OutFile` is intentionally fixed.

## Editing Rules

When changing the installer:

- edit `scripts/installer.nsi` and keep both `.github/workflows/feature-windows.yml` and `.github/workflows/master-windows.yml` invoking it the same way
- pass the version through `-DAPP_VERSION="${VERSION}"` so the installer, uninstaller, and Programs and Features stay in sync with `CCM_APP_VERSION`
- keep installer assets that should not be generated at runtime (such as `installer_icon.ico`) committed in `scripts/`
- keep shortcut display names version-agnostic so upgrades do not orphan old shortcuts
- if you add a new registry value to the uninstall key, mirror it in the `Section "Uninstall"` cleanup if it lives outside that key

## Troubleshooting

- **`makensis: command not found`:** install NSIS and ensure `makensis` is on `PATH`. In CI this is provided by the MSYS2 package `mingw-w64-ucrt-x86_64-nsis`.
- **`File: ... \build\bin\*.*` failures:** the build payload is missing. Run `cmake --build build --parallel` first and confirm `build/bin/ccm3.exe` plus the runtime DLLs exist (see [Build Locally Guide](dow-doc-build-locally.md)).
- **Installer self-labels as `localbuild` in CI:** `-DAPP_VERSION="${VERSION}"` was not passed, or `${VERSION}` was empty. Check that the workflow's version-computation step ran before the installer step and exported `VERSION`.
- **Programs and Features does not show a version:** the `DisplayVersion` registry write was skipped because the installer was built without `APP_VERSION` (or with an empty value). Rebuild with the define set.
- **Old shortcuts left behind after upgrade:** shortcut display names were changed (or were made version-specific). Restore the version-agnostic names so upgrades overwrite cleanly.
- **Uninstaller appears to leave files:** `RMDir /r "$INSTDIR"` does not remove files outside `$INSTDIR`. Anything written by the app at runtime under user profile paths is intentionally kept; the uninstaller only manages what the installer placed.

## Related Docs

- [CI/CD Guide](ci-cd-guide.md)
- [Versioning Guide](versioning.md)
- [Build Locally Guide](dow-doc-build-locally.md)
