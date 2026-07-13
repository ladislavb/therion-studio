# Repository Review Implementation Plan

Date: 2026-07-13

Status: active orchestration plan for findings in `plans/REVIEW_CODEX.md`.

Scope: turn the current repository review into bounded implementation slices that can be executed safely by a smaller
model. This file owns ordering and cross-plan dependencies. Detailed design and verification stay in the focused plans
linked below.

## Operating Rules For Smaller Models

Every implementation turn shall:

1. select exactly one numbered slice from one focused plan;
2. read that plan, `AGENTS.md`, and the listed target files before editing;
3. record the baseline focused-test result before changing behavior;
4. edit only the slice's declared production, test, build, and documentation scope;
5. stop and report instead of guessing when a listed precondition is false or ownership differs from the plan;
6. preserve compatibility fallbacks until the slice's removal gate is satisfied;
7. run the slice verification plus `python3 scripts/check_structure_constraints.py` and `git diff --check`;
8. update the focused plan and `WORKLOG.md` after the slice;
9. request explicit confirmation before committing.

A slice shall not be combined with opportunistic renames, broad file splitting, generated-resource cleanup, UI restyling,
or parser/source-model changes. User-visible behavior changes additionally require the relevant specification, manual,
localization, and acceptance-criteria updates in the same slice.

## Finding To Plan Map

| Review finding | Owning plan | First executable slice | Dependency |
| --- | --- | --- | --- |
| P1-1 non-hermetic core runner | `TEST_HERMETICITY_PLAN.md` | H1 committed minimal `.lox` fixture | none; do first |
| P1-3 stale Structure/Outputs results | `PROJECT_ASYNC_COORDINATION_PLAN.md` | A1 request serial contract | hermetic baseline |
| P1-2 blocking SQL import/query | `SQL_REPORT_ASYNC_PLAN.md` | S1 worker-owned database contract | hermetic baseline |
| P1-5 UI-thread project watcher walk | `PROJECT_ASYNC_COORDINATION_PLAN.md` | W1 pure watch inventory | A1-A3 recommended first |
| P1-7 localization extraction blind spot | `LOCALIZATION_EXTRACTION_PLAN.md` | L1 fix known visible literals | none; separate commit chain |
| P1-6 static Map resource/cache state | `MAP_RUNTIME_OWNERSHIP_PLAN.md` | R1 style catalog provider seam | scanner/SQL work independent |
| P1-4 synchronous Map full rebuild | `MAP_PARTIAL_REFRESH_PLAN.md` | M0 immutable projection handoff | R1 style injection first |

Secondary findings are handled as follows:

- P2-1 is covered by the SQL workflow plan, Map resource plan, and touched-area GUI/controller extractions. It is not a
  license for a repository-wide service rewrite.
- P2-2 requires a measured Map undo-memory baseline before changing the safe full-snapshot transaction fallback. It is
  tracked as the independent U-series in `MAP_RUNTIME_OWNERSHIP_PLAN.md` and must remain separate from resource and scene
  refresh commits.
- P2-3 is opportunistic responsibility-based splitting only. Each focused plan names the extraction boundary it needs.
- P2-4 is handled by `TEST_HERMETICITY_PLAN.md` and by migrating only touched legacy tests when the runtime boundary is
  clear.
- P2-5 remains a focused, behavior-preserving infrastructure-placement backlog item. Do not combine it with P1 work.
- P2-6 is addressed by keeping `WORKLOG.md` short and using this plan as the detailed active queue.

## Delivery Order

### Gate 1 — Restore Trustworthy Verification

Complete `TEST_HERMETICITY_PLAN.md` H1-H3. The mandatory `unit` label must be independent of ignored local corpora.

Exit gate:

- a checkout with no `sample_data/` passes;
- a checkout with a partial `sample_data/` passes the mandatory suite;
- opt-in corpus coverage reports per-fixture skips without changing mandatory results.

### Gate 2 — Make Worker Results Monotonic

Complete `PROJECT_ASYNC_COORDINATION_PLAN.md` A1-A3 for Structure and Outputs before widening project scan caches or
enabling more automatic work.

Exit gate:

- request B submitted while A is running prevents A from being published;
- latest same-root and changed-root requests are both covered;
- teardown does not publish into destroyed consumers.

### Gate 3 — Remove Unbounded UI-Thread Work

Complete SQL S1-S4, then watcher W1-W4. SQL and watcher work shall be separate commit chains because their cancellation,
thread affinity, and user-facing states differ.

SQL S5 (preset persistence and CSV extraction) is a lower-priority dependency-injection follow-up. It is not required
to close the GUI-thread responsiveness gate and shall remain a separate commit chain.

Exit gate:

- report import/query and recursive watcher inventory do not run on the GUI thread;
- cancellation, supersession, stale-result suppression, and teardown have focused tests;
- UI error and busy states remain actionable and translatable.

### Gate 4 — Close Localization Gaps

Complete localization L1-L3. The extraction audit must use a temporary catalog and must not rewrite committed `.ts`
files as part of checking.

### Gate 5 — Make Map Runtime Inputs Explicit

Complete Map resource R1-R5. Style/catalog ownership comes before broad Map partial refresh so render contexts no longer
reach into hidden IO/settings state.

### Gate 6 — Reduce Map Refresh Cost Incrementally

Follow the revised `MAP_PARTIAL_REFRESH_PLAN.md`: immutable projection handle, scene generation contract, measured
single-feature replacement, then dependency-aware widening. Preserve full rebuild as the fallback.

## Active Plan Scope Audit

| Plan | Review status | Scope decision |
| --- | --- | --- |
| `PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` | reviewed and refreshed | Keep snapshot/cache and live-validation scope; monotonic scanner results and watcher discovery move to `PROJECT_ASYNC_COORDINATION_PLAN.md`. |
| `MAP_PARTIAL_REFRESH_PLAN.md` | reviewed and refreshed | Keep delta-rendering scope; the implemented DOM projection baseline and P1-4 prerequisites are now explicit. |
| `SVG_BACKGROUND_PLAN.md` | related through P1-6 | Keep SVG compatibility/UX scope; shared cache/provider ownership belongs to `MAP_RUNTIME_OWNERSHIP_PLAN.md`. |
| `3D_VIEWER_PLAN.md` | scope valid | Keep read-only viewer refinement; mandatory versus optional `.lox` fixtures are governed by `TEST_HERMETICITY_PLAN.md`. |
| `GUI_CLEANUP.md` | scope valid | Keep behavior-preserving presentation cleanup; do not absorb SQL, watcher, Map cache, or localization workflow ownership into visual cleanup. |
| `archive/UNIFIED_SOURCE_DOM_PLAN.md` | completed archive | Verification/history only; never use as an active queue. |
| `archive/UNIFIED_SOURCE_DOM_INVENTORY.md` | completed archive | Historical inventory only; current direct-parser exceptions are enforced by structure constraints. |

## Global Verification Matrix

- Every slice: focused tests, structure constraints, `git diff --check`.
- Async project work: supersession, cancellation, same-root replacement, changed-root replacement, teardown.
- SQL work: large import, malformed rollback, query deadline/interruption, stale suppression, CSV parity.
- Localization work: temporary `lupdate` extraction, Czech/Slovak completeness, placeholders, known runtime surfaces.
- Map resource/refresh work: repeated parallel UI tests, large-scene smoke, selection/drag/undo, background lifecycle, stale
  generation teardown.
- Cross-platform-sensitive work: Debug builds and focused tests on macOS, Windows, and Linux CI before closure.

## Completion Rule

A review finding may be marked resolved in `REVIEW_CODEX.md` only when its focused plan exit gate is met, tests are in
the repository, and any changed architecture/behavior documentation is current. Completing a design seam without moving
the production caller does not close the finding.
