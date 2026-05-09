#documentation #testing #quality

# Testing Guide And Test Code Of Conduct

This guide defines how testing works in Card Collection Manager 3 and which standards test code must meet. For local build setup and toolchain prerequisites, see [Build Locally Guide](dow-doc-build-locally.md).

**Quick Setup:** run `ccm_core_tests` from a clean build, keep tests hermetic, and update contract tests in the same change when contracts move.

## Testing Focus

The project prioritizes deterministic, fast, behavior-oriented testing. Most automated coverage intentionally targets `core/` logic and infrastructure/service behavior, while UI validation remains manual.

## Test Setup

- framework: `doctest`
- primary target: `ccm_core_tests`
- location: `tests/`
- default behavior: tests enabled via `CCM_BUILD_TESTS=ON`

## Run Tests

From repository root:
```bash
cmake -S . -B build -DCCM_BUILD_TESTS=ON
cmake --build build --target ccm_core_tests
ctest --test-dir build --output-on-failure
```

Windows and Linux use the same logical flow; only generator and compiler setup differ.

## Coverage Surface

**SonarCloud:** The CI Sonar scan reports coverage against `core/` paths that `ccm_core_tests` can execute. `ui_wx/` and the `app/` composition root are excluded from Sonar’s **coverage** calculation (`sonar.coverage.exclusions`) because they are not run under the doctest suite; UI behavior is covered by manual validation below. Other Sonar metrics still include those directories.

Current automated tests cover non-UI behavior, including:

- filesystem naming and parsing behavior
- domain JSON round-trip behavior
- service behavior (`CollectionService`, `ConfigService`, `SetService`)
- repository behavior with in-memory filesystem fakes
- game set-source parsing behavior

## Manual UI Validation

Because there is no UI automation, UI-affecting changes require manual checks:

- theme switching (dark and light)
- dialog and popup behavior
- add/edit/delete card flows
- set update flows
- preview and image interactions

For theming work, rebuild and run the final app target (`ccm`) instead of validating only static library targets.

## Test Code Of Conduct

### Keep Tests Hermetic

- do not call real network services
- do not depend on local machine files
- use fakes and in-memory adapters where possible

### Test Behavior, Not Internals

- assert externally visible outcomes
- avoid brittle assertions tied to incidental implementation details
- prefer domain-level expectations over call-level trivia

### Keep Tests Deterministic

- no unseeded randomness
- no timing-sensitive assertions that can flap
- no ordering assumptions unless ordering is part of the contract

### Keep Tests Readable

- one intent per test case
- descriptive test names
- clear arrange/act/assert flow
- minimal abstraction for small tests

### Update Tests With Contract Changes

When contracts change, update tests in the same change:

- domain JSON schema or aliases -> round-trip tests
- filename formatting or parsing -> filesystem naming tests
- service semantics -> matching service tests

Behavior changes without aligned tests are incomplete.

### Avoid Over-Mocking

- prefer realistic fakes over mock-heavy tests
- mock at external boundaries only when needed
- preserve confidence in integration-shaped behavior paths

### Keep Runtime Practical

- keep suite runtime fast enough for frequent local execution
- avoid repeated expensive setup when shared setup works
- justify any expensive new suite and keep scope narrow

## Review Checklist

Before merging test changes, verify:

- tests pass locally
- no new flakiness risk
- no external dependency introduced
- assertions reflect intended behavior
- failure messages are clear and actionable

## Related Docs

- [Build Locally Guide](dow-doc-build-locally.md)
- [Intro For New Developers](intro-to-new-developers.md)
- [Adding a new game to Card Collection Manager](adding-a-new-game.md)
