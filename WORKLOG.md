# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. Windows map-input validation for the deferred vertex-refresh fix.
2. Unified Source DOM consumer migration in small, tested slices.
3. Release readiness for `v2026.6.9` without broad map/editor rewrites.
4. Plan-driven follow-ups for GUI cleanup, SVG backgrounds, and 3D viewer refinement.

## Active Work

### Release Readiness / Windows Map Input

- Send the next Windows build with the panning update, diagnostic startup header, per-pan stage timings, and
  `map-scene-refresh` diagnostics to the tester, and request the same mouse/stylus workflow with
  `THERION_STUDIO_ENABLE_LOG=1`.
- Confirm the next follow-up log keeps `Move line Vertex` source transactions near the current sub-150 ms range with
  `policies_ms=0`, and compare panning `elapsed_ms` before/after the reduced per-move command-surface refresh.
- Confirm the indexed map vertex selection restore lowers post-edit `map-scene-refresh selection_ms`; only then decide
  whether remaining Windows lag needs `clear_ms` or `final_ui_ms` optimization.
- Use `MapEditorLargeScenePerformanceSmokeTest` as the local generated large-map harness when comparing map refresh
  changes; it targets roughly 2k parsed lines and 6k scene items without relying on external cave data.
- Validate the next Windows build by feel rather than asking for another diagnostic log: simple unstyled open-line vertex
  moves avoid forcing a full scene rebuild immediately after the source transaction, while styled/decorated line edits
  still keep the full refresh needed for generated block/slope/decorator geometry.
- Keep the decorated-line preview path covered before widening the no-full-refresh optimization: `wall:blocks` line-point
  segment guide paths now update during vertex preview movement, but full scene refresh remains the correctness fallback
  for styled/decorated commits.
- Use `plans/MAP_PARTIAL_REFRESH_PLAN.md` for the detailed slice queue before widening styled/decorated line commits from
  full scene refresh to safe one-line item-group refresh.
- Keep broad Map/TH2 projection rewrites out of release stabilization until Windows feedback confirms the deferred
  vertex-refresh fix is stable.
- Before tagging or packaging handoff, run local validation focused on recent map-input, source-transaction, installer,
  QML/Qt Quick deployment, and release-note changes.
- Keep release notes, README, package metadata, CI artifact workflow, and prerelease package labels aligned with
  `v2026.6.9`.

### Unified Source DOM / Transactions

- Use `plans/UNIFIED_SOURCE_DOM_PLAN.md` as the detailed slice queue.
- Next implementation slice: migrate one Raw cursor-token consumer to `TherionSourceLogicalDocument::tokenAtOffset()`
  while preserving quoted-token, option-token, comment, continuation-row, and end-of-line behavior.
- After the Raw cursor slice, prefer one read-only Blocks details consumer before touching Map scene refresh or geometry
  projection.
- Keep Therion namespace/reference changes behind `docs/THERION_COMPATIBILITY.md` coverage, especially
  `object@child.parent` qualified-reference order.
- Keep source transaction ownership work incremental: one caller or workflow per commit, with explicit result handling,
  undo label, revision behavior, projection invalidation, dirty-state behavior, and selection/cursor restoration.
- Do not delete token-line compatibility APIs until Map geometry and legacy tests have replacement coverage.

### Validation And Catalog Metadata

- Treat full-project validation as an explicit/manual workflow by default; use the Settings toggle only when projects are
  small enough or the user wants live full-project diagnostics.
- Ask large-project Windows testers to collect `THERION_STUDIO_ENABLE_LOG=1` output and compare
  `project-validation-scan` versus `project-validation-ui` timings before changing more validation architecture.
- Use `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` for the DOM-first project source snapshot slice queue; current
  findings show Structure and Validation can duplicate project-index scans, and Validation also rebuilds local file
  diagnostics every run.
- Build the next validation/project-scan slice on the shared project source request key so Structure and Validation can
  reuse one normalized source snapshot instead of deriving independent scan identities.
- Use the project source snapshot collector as the next integration point for scanner migration; keep the first runtime
  hookup behavior-preserving by comparing Structure and Validation outputs before removing their local traversal code.
- Prefer Settings -> troubleshooting logs for tester builds: the preference is time-limited, restart-applied, and uses
  rotated application log files instead of requiring users to set environment variables.
- Optimize future live validation with incremental file/revision caching, generation-keyed cancellation, and cheaper
  Validation tree updates before making automatic full-project validation the default again.
- Keep validation conservative while moving command, option, and positional argument interpretation into catalog-backed
  logical-command metadata.
- Keep problem reporting centralized in the Validation panel while Structure remains an orientation/navigation view.
- Prioritize regression coverage for any namespace, duplicate-identifier, reference-resolution, or validation-fix change.

### Test And Structure Hygiene

- Use QTest for new C++ tests while keeping CTest as the runner.
- Keep `tests/core/` and `TherionCoreQTests` as the baseline pattern for small core-only QTest cases.
- Keep app/service/editor aggregate QTest runners grouped by dependency/runtime boundary rather than creating one
  executable per tiny class.
- Continue migrating touched hand-rolled tests to QTest where the dependency/runtime boundary is already clear.
- Keep `python3 scripts/check_structure_constraints.py` green and preserve guardrails against map-editor source mutation
  bypasses.
- Keep optional sample-data dependent tests from aborting CI when fixture directories are absent.
- Keep Linux/Windows CI and package Qt runtime module lists aligned with QML inspector imports.
- Keep UI smoke tests deterministic across platform event-loop timing differences.
- When touching source-driven map scene refresh, repeat `MapEditorDragUndoRedoSmokeTest` to guard delayed refresh
  selection restoration for cursor-derived line/area ownership such as `endline` and `endarea`.
- Keep explicit user confirmation before every `git commit`.

### UI Cleanup

- Follow `plans/GUI_CLEANUP.md` for slice order.
- Next implementation slice should be one style extraction or repeated-metric cleanup, not a broad UI rewrite.
- Keep style policy, UI construction, presentation contracts, and source/model logic separated.
- Do not combine GUI cleanup with source-model, parser, validation, file IO, or process execution changes.

### SVG Backgrounds

- Follow `plans/SVG_BACKGROUND_PLAN.md` for remaining SVG background work.
- Next implementation slice should audit current SVG labels, inspector Gamma state, Fit With Background behavior, and
  failed-load reporting before adding new abstractions.
- Preserve Mapiah `format=svg` semantics and existing background source transaction paths.

### 3D Viewer

- Follow `plans/3D_VIEWER_PLAN.md` for remaining work.
- Next implementation slice should add or identify a real Therion-exported `.lox` fixture with terrain surface chunks,
  or add debug/log load/render statistics before renderer refactoring.
- Keep the viewer read-only and keep `.lox` loading/model/statistics in core.

## Blocked / Needs Input

- Windows map-input follow-up log from the tester.
- Old Therion/Metapost crash fixture: parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Replace remaining fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Implement safe one-line map partial refresh according to `plans/MAP_PARTIAL_REFRESH_PLAN.md` after release stabilization.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- After the project source snapshot collector is wired into one scanner, share Structure/Validation project-index snapshots
  and add per-file validation cache according to `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md`.
- Restore automatic full-project validation as the recommended/default mode only after live diagnostics are incremental,
  cancellable, and UI-cheap for nested projects.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Bounded `.xvi` cache policy for very large projects.
- Station marker/label priority ranking follow-up: tune automatic decluttering if dense projects hide important stations.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX follow-up for hardware-specific pressure, hover, and tablet-driver behavior.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, stable raster position anchors during scale/rotation, XTherion rewrite caveats, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
