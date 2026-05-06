# AGENTS.md

Utility scripts and CI helper assets live in this folder.

## Scope

- Keep CI-facing scripts and config here so workflows stay concise.
- Current contents:
  - `installer.nsi` — NSIS installer definition used by the Windows CI job.
  - `installer_icon.ico` — committed installer icon consumed by `installer.nsi`.

## Editing rules

- Prefer referencing script files from workflows instead of embedding large inline scripts.
- Keep scripts deterministic and non-interactive so CI can run unattended.
- When changing installer behavior, edit `installer.nsi` and ensure `.github/workflows/build-and-test.yml` still invokes it via `makensis scripts/installer.nsi`.
- Keep installer assets that should not be generated at runtime (such as icon files) committed in this folder.
- If a script depends on generated files, document those expectations in the script or workflow step that creates them.
