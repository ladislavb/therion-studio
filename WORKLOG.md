# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. `2026.7.1` development planning and first implementation slices after `v2026.6.9`.
2. Unified Source DOM consumer migration in small, tested slices.
3. Project source snapshot and validation/cache reuse work for repeated project scans.
4. Plan-driven follow-ups for map partial refresh, GUI cleanup, SVG backgrounds, and 3D viewer refinement.

## Active Work

### 2026.7.1 Planning

- Start the post-`v2026.6.9` cycle with architecture-aligned slices that reduce duplicate parsing, repeated project
  scanning, and source-transaction drift without broad parser or map-rendering rewrites.
- Prefer one focused implementation slice per commit, with matching tests and `python3 scripts/check_structure_constraints.py`
  before proposing a commit.
- Keep user-visible feature work aligned with `SPECIFICATION.md`; update the specification and `docs/USER_MANUAL.md` in the
  same change when behavior or workflows change.

### Unified Source DOM / Transactions

- Use `plans/UNIFIED_SOURCE_DOM_PLAN.md` as the detailed slice queue.
- Raw completion prefix detection now uses shared logical token ranges for parsed cursor tokens while preserving the
  existing completion-character filtering for path and partial-token behavior; keep this covered in
  `TextEditorRawEditorQTests`.
- Blocks canvas data-body scanning now uses shared logical commands when deciding where data rows end, so continuation
  rows after a data body do not get reparsed as standalone raw lines.
- Blocks details selection loading now reads selected logical commands from a source snapshot when populating read-only
  fields and option rows for continued commands.
- Map details panel line-action, line-option, and line-point read-only feature lookups now consume
  `TherionSourceLogicalDocument` commands through `MapEditorSourceReferenceResolver` instead of reparsing the full editor
  text for each lookup.
- Map area-reference lookups now have logical-command resolver overloads, and the object-details delete guard reads the
  "Used by area" state from the shared logical source projection instead of reparsing editor text directly.
- Map selection and inspector-object area-reference consumers now receive revision-cached logical commands from
  `MapEditorTab` instead of reparsing editor text for border-line highlighting and delete-blocked state.
- The object-details delete guard now uses the same logical-command area-reference resolver before mutating source text,
  avoiding the last map area-reference lookup that reparsed editor text directly.
- The object-details panel now reuses the map tab's revision-cached logical commands for read-only selected-command,
  line-feature, area-reference, quick-field, and scrap-scale lookups instead of creating local source snapshot caches per
  field refresh.
- Line-extension start now resolves the selected endpoint feature from the map tab's revision-cached logical commands,
  while the commit path still uses explicit before/after source text for the rewrite transaction.
- Canvas line-vertex owner selection restore now resolves line features from the map tab's revision-cached logical commands
  instead of reparsing editor text during restore.
- Canvas partial-refresh feature resolution after a source edit now also uses revision-cached logical commands; rewrite
  planning paths still use explicit before/after source text snapshots.
- Map controller contexts now share a small `MapEditorLogicalSourceContext` for revision-cached logical command access
  instead of each owning an identical callback field.
- Next implementation slice: pause DOM migration and switch to the next `2026.7.1` performance/UX item, or start a
  dedicated TH2 geometry projection design slice before touching rewrite planners.
- Keep Therion namespace/reference changes behind `docs/THERION_COMPATIBILITY.md` coverage, especially
  `object@child.parent` qualified-reference order.
- Keep source transaction ownership work incremental: one caller or workflow per commit, with explicit result handling,
  undo label, revision behavior, projection invalidation, dirty-state behavior, and selection/cursor restoration.
- Do not delete token-line compatibility APIs until Map geometry and legacy tests have replacement coverage.

### Validation And Catalog Metadata

- Treat full-project validation as an explicit/manual workflow by default; use the Settings toggle only when projects are
  small enough or the user wants live full-project diagnostics.
- Use `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` for the DOM-first project source snapshot slice queue; current
  findings show Structure and Validation now share snapshot-compatible collection/index input paths, while repeated
  requests still need explicit cache ownership.
- `ProjectSourceProjectionCache` now provides the first focused per-run source/logical projection cache for project source
  snapshots with observable reuse stats.
- `ProjectValidationScanner` local per-file validation now uses `ProjectSourceProjectionCache` for source and
  catalog-aware logical projections.
- Project validation troubleshooting logs now include project source projection cache build/hit counts.
- Unindexed TH2 station-reference diagnostics now reuse the project source projection cache instead of building a second
  scanner-local logical source projection.
- `ProjectValidationScanner::Result` now exposes projection cache stats for tests and diagnostics, including coverage that
  the unindexed TH2 diagnostic helper reuses source projections after local validation.
- Project-index scans now expose internal logical-document cache stats through validation results, with coverage that
  diagnostic helpers reuse the same per-file logical projections after structure collection.
- Project validation now retains source and catalog-aware logical projections across repeated scans of the same project
  and validation catalog, keyed by project root, catalog signature, file path, source type, loaded state, and content hash.
- Project-index validation scans now consume validation-provided plain logical projections for loaded project sources
  instead of rebuilding them inside `ProjectStructureIndex`.
- Repeated validation scans now reuse a retained `ProjectIndexSnapshot` when the collected project source request key is
  unchanged, avoiding repeated map/join/station/duplicate project-index passes for manual refreshes with identical input.
- Repeated validation scans now reuse per-file validation findings when a document's path, content hash, source type,
  loaded state, validation catalog signature, and known project file set are unchanged.
- Runtime diagnostic logs confirm repeated manual validation refreshes now report full `document_validation_cache_hits`
  after the first scan, reducing local validation work from tens of milliseconds to near-zero for unchanged projects.
- `ProjectScanCacheService` now owns the retained project-index snapshot cache behind an explicit app-service dependency
  shared by Structure and Validation scanners at the `MainWindow` composition boundary.
- `ProjectScanCacheService` now also reuses the last collected project source snapshot for unchanged project source
  request keys, so repeated Structure/Validation scans can skip redundant source discovery and text collection.
- Validation UI now skips rebuilding the problem tree and reapplying open-editor diagnostics when a project-validation
  result signature is unchanged, while still honoring reveal and post-fix navigation requests.
- Manual project-validation refreshes now keep existing project results visible while a refresh is running when a prior
  project-validation signature exists, allowing unchanged follow-up results to use the model-skip path.
- Next validation/project-scan slice: add cancellation/generation handling for superseded live-validation requests or
  gather a fresh diagnostic log to identify the next non-cache bottleneck; do not reintroduce scanner-local file traversal,
  broad context bags, or static/global cache state.
- Prefer Settings -> troubleshooting logs for tester builds: the preference is time-limited, restart-applied, and uses
  rotated application log files instead of requiring users to set environment variables.
- Optimize future live validation with incremental file/revision caching, generation-keyed cancellation, and cheaper
  Validation tree updates before making automatic full-project validation the default again.
- Keep validation conservative while moving command, option, and positional argument interpretation into catalog-backed
  logical-command metadata.
- Keep problem reporting centralized in the Validation panel while Structure remains an orientation/navigation view.
- Prioritize regression coverage for any namespace, duplicate-identifier, reference-resolution, or validation-fix change.

### Map Input And Partial Refresh

- Keep `plans/MAP_PARTIAL_REFRESH_PLAN.md` as the detailed slice queue for widening one-line map partial refresh.
- Use `MapEditorLargeScenePerformanceSmokeTest` as the local generated large-map harness when comparing map refresh
  changes; it targets roughly 2k parsed lines and 6k scene items without relying on external cave data.
- Verify the next Windows build by feel for mouse/stylus workflows; only ask for another diagnostic log if the tester still
  sees lag, segmentation, or selection instability.
- Windows map-input follow-up log showed single-touch events being accepted by the map input controller while no draw,
  pan, or tablet interaction was active; the current stabilization slice lets those single-touch begin/update events pass
  through instead of suppressing platform handling.
- Line partial refresh now keeps one-line item replacement on the existing source-bounds projection and falls back to a
  full scene refresh when a vertex edit expands map source bounds, preventing mixed projection coordinates.
- `map-line-partial-refresh` diagnostics now include previous, current, and render source bounds so fallback logs show
  whether bounds changed, were missing, or stayed projection-compatible.
- Point and single-vertex line/area geometry moves now pass their planned `TherionSourceTextEdit` ranges into the shared
  source transaction controller so the initial apply path edits the touched source range instead of replacing the full
  document snapshot.
- Line vertex selection restore now short-circuits duplicate same-vertex/same-coordinate recovery calls before refreshing
  geometry presentation, command/help state, or the details panel; diagnostics report this as `skipped_ui=1`.
- Point and line/area geometry handles now require an actual mouse-move drag before committing a source geometry move, so
  repeated clicks or interleaved trackpad wheel events cannot create a false `Move line Vertex` transaction.
- If diagnostics are needed, use `map-line-partial-refresh`, `map-line-selection-restore`, `line-area-anchor-release`, and
  `line-double-click-complete` lines to separate partial-refresh fallback, selection restore, and Bezier draft issues.
- Keep broad Map/TH2 projection rewrites out of map-input follow-up work until Windows feedback confirms the deferred
  vertex-refresh and one-line partial-refresh paths are stable.
- Keep decorated-line preview, line-label preview, Bezier control precision, and SKBB-style decoration regressions covered
  when tuning map rendering or widening partial refresh.

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

- Old Therion/Metapost crash fixture: parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Replace remaining fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Complete safe one-line map partial refresh according to `plans/MAP_PARTIAL_REFRESH_PLAN.md` after release stabilization.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Add project-index snapshot cache ownership and per-file validation cache according to
  `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md`.
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
