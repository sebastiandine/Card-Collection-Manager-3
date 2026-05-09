# Testing Guide and Test Code of Conduct

This document explains how testing works in Card Collection Manager 3 and defines expectations for writing and maintaining tests.

## Testing philosophy

The project prioritizes:

- deterministic tests
- fast feedback
- behavior-focused coverage of core logic
- no reliance on network or real filesystem for unit tests

Most automated coverage is intentionally concentrated in `core/` and infra/service behavior. UI behavior is validated manually.

## Current test setup

- test framework: `doctest`
- test target: `ccm_core_tests`
- location: `tests/`
- default build behavior: tests enabled via `CCM_BUILD_TESTS=ON`

## How to run tests

From repository root:

```bash
cmake -S . -B build -DCCM_BUILD_TESTS=ON
cmake --build build --target ccm_core_tests
ctest --test-dir build --output-on-failure
```

Windows and Linux follow the same logical flow; only generator/toolchain setup differs.

For complete local environment setup, see `dow-doc-build-locally.md`.

## What is covered

Current suite covers the non-UI surface, including:

- filesystem naming/parsing behavior
- domain JSON round-trips
- service behavior (`CollectionService`, `ConfigService`, `SetService`, etc.)
- repository behavior over in-memory filesystem fakes
- game set-source parsing behavior

## Manual testing expectations (UI)

Since there is no UI automation yet, validate UI changes manually:

- dark/light theme behavior
- dialog and popup behavior
- add/edit/delete card flows
- set update flows
- preview/image interactions

When changing UI theming, prefer rebuilding/running the final app target (`ccm`) instead of only static library targets.

## Test Code of Conduct

These are non-negotiable quality rules for test code in this repository.

## 1) Keep tests hermetic

- Do not hit real network services.
- Do not depend on local machine files.
- Use test doubles/fakes (for example in-memory filesystem) whenever possible.

## 2) Test behavior, not implementation trivia

- Assert externally visible outcomes.
- Avoid brittle tests that break on harmless refactors.
- Prefer meaningful domain assertions over incidental internal details.

## 3) Keep tests deterministic

- No random behavior without fixed seed.
- No time-sensitive assertions that can flap.
- No order assumptions unless order is part of the contract.

## 4) Keep tests readable and maintainable

- One clear intent per test case.
- Use descriptive test names.
- Keep arrange/act/assert flow obvious.
- Avoid over-abstracting tiny tests.

## 5) Update tests with contract changes

If you change a contract, update tests in the same change:

- domain JSON schema/aliases -> round-trip tests
- filename formatting/parsing -> fs naming tests
- service semantics -> corresponding service tests

Do not merge behavior changes without aligned tests.

## 6) Avoid over-mocking core behavior

- Prefer realistic fakes over mocking every call.
- Mock only at external boundaries where needed.
- Preserve confidence that real integration paths are still represented.

## 7) Keep runtime practical

- Tests should stay fast enough for frequent local execution.
- Avoid expensive setup in each case when shared setup is sufficient.
- If a new suite is expensive, justify it and keep scope tight.

## Review checklist for test changes

Before finalizing a test PR/change, verify:

- tests pass locally
- no new flakiness risk
- no external dependencies introduced
- assertions reflect intended behavior
- failure messages are clear enough to debug quickly

## Related docs

- `dow-doc-build-locally.md` — local build/test setup
- `intro-to-new-developers.md` — onboarding and architecture context
- `adding-a-new-game.md` — when extending per-game functionality
