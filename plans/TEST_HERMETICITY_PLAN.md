# Test Hermeticity Plan

Date: 2026-07-13

Review findings: P1-1, P2-4.

Status: active. This is the first implementation plan in `REVIEW_IMPLEMENTATION_PLAN.md`.

Scope: make mandatory CTest labels deterministic in a clean checkout while retaining optional real-project corpus
coverage as explicitly opt-in integration tests.

## Current Failure Contract

`tests/core/ThreeDViewerLoxLoaderTest.cpp` treats any local `sample_data/` directory as evidence that a fixed Babice
fixture exists. A partial ignored corpus therefore fails `TherionCoreQTests`, while a checkout without the directory
skips the same test. Mandatory unit coverage must not depend on ignored developer data.

## Non-Goals

- Do not add a large or license-unclear real-project corpus to Git.
- Do not weaken mandatory `.lox` loader assertions into unconditional skips.
- Do not put corpus, GPU, UI, packaging, or performance work into the `unit` label.
- Do not split the complete core aggregate runner in this plan.
- Do not change `.lox` parser behavior unless the committed minimal fixture exposes a real defect.

## H1 — Establish A Committed Minimal Fixture

Allowed scope:

- `tests/fixtures/three_d_viewer/**`
- `tests/core/ThreeDViewerLoxLoaderTest.cpp`
- `CMakeLists.txt` only if the fixture root must be supplied explicitly

Steps:

1. Determine whether a minimal fixture can be generated deterministically from known bytes already represented by the
   synthetic loader tests.
2. If storing a binary fixture, document its generator/provenance and license in the fixture directory.
3. Make one mandatory test load only this committed fixture and assert a small stable scene contract: successful load,
   expected object counts, bounds, and one semantic flag/name.
4. Resolve the fixture path from an explicit repository/test-fixture root, not the current working directory alone.

Stop conditions:

- If provenance or redistribution rights cannot be established, do not copy a real cave fixture; generate a minimal
  synthetic `.lox` fixture instead.
- If the loader format cannot produce a stable minimal file without parser changes, stop and document the format gap
  before editing production code.

Verification:

- `TherionCoreQTests` with no `sample_data/` present;
- the focused loader case from a build-directory working directory;
- `git diff --check`.

## H2 — Separate Optional Corpus Coverage

Allowed scope:

- `tests/core/ThreeDViewerLoxLoaderTest.cpp`, or a new isolated corpus test when aggregate-runner semantics would be
  clearer
- `CMakeLists.txt`
- `docs/BUILDING.md` for the opt-in invocation/environment contract

Steps:

1. Represent each optional real-project fixture as an independent data row.
2. Skip a row only when that row's file is absent; the presence of a parent directory shall not make another row
   mandatory.
3. Put broad corpus coverage behind an explicit CTest label or environment/option contract that is off for normal
   `unit` runs.
4. Keep malformed synthetic fixtures and the committed minimal fixture in mandatory core coverage.

Acceptance:

- no corpus: mandatory tests pass, corpus test reports an intentional skip or is not selected;
- partial corpus: available rows execute, absent rows skip independently, mandatory result is unchanged;
- complete corpus: all configured rows execute.

Stop condition: do not add a custom test runner when QTest data rows and CTest labels provide the required boundary.

## H3 — Add The Partial-Corpus Regression

Add a focused test of fixture discovery policy rather than mutating the developer's actual `sample_data/` directory.
Prefer extracting a small pure fixture-resolution function if that is the smallest deterministic seam.

Acceptance:

- a root containing unrelated sample data does not make the fixed optional path mandatory;
- equivalent absolute/repository-relative fixture roots resolve consistently;
- test execution does not write outside its temporary directory.

## H4 — Touched-Test Migration Rule

When touching another legacy hand-rolled test for a review implementation slice:

- migrate it to an existing aggregate QTest runner if the dependency/runtime boundary already matches;
- keep a separate executable for UI-heavy, process-backed, timing-sensitive, performance, or crash-containment tests;
- do not create one executable per small test class;
- never combine a test-framework migration with production behavior changes unless needed for deterministic coverage.

## Exit Gate

- `ctest -L unit` is green with absent, partial, and complete optional local corpus states.
- Mandatory loader coverage uses only repository-owned deterministic fixtures.
- Optional corpus invocation and skip behavior are documented.
- `3D_VIEWER_PLAN.md` no longer treats ignored sample data as a mandatory verification baseline.
