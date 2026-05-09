# AGENTS.md

Utility scripts and CI helper assets live in this folder.

## Scope

- Keep CI-facing scripts and config here so workflows stay concise.
- Current contents:
  - `installer.nsi` — NSIS installer definition used by the Windows CI job.
  - `installer_icon.ico` — committed installer icon consumed by `installer.nsi`.
  - `compute_feature_version.sh` — branch+sha version formatter for feature CI workflows.
  - `compute_master_semver.sh` — semantic version bump logic for merged PRs to `master`.

## Editing rules

- Prefer referencing script files from workflows instead of embedding large inline scripts.
- Keep scripts deterministic and non-interactive so CI can run unattended.
- Keep versioning scripts strict and fail-fast (non-zero exit) when required title/version conventions are violated.
- When changing installer behavior, edit `installer.nsi` and ensure both `.github/workflows/feature-windows.yml` and `.github/workflows/master-windows.yml` still invoke it via `makensis scripts/installer.nsi`.
- Keep installer assets that should not be generated at runtime (such as icon files) committed in this folder.
- `installer.nsi` path semantics:
  - `installer_icon.ico` is resolved relative to `scripts/`.
  - build payload is loaded from `..\build\bin\*.*` (project root build output).
  - installer output is written to `..\ccm3-windows-installer.exe` so CI upload paths can stay root-based.
- If a script depends on generated files, document those expectations in the script or workflow step that creates them.
