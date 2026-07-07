# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. `2026.7.2` development planning and first implementation slices after `2026.7.1`.
2. Unified Source DOM consumer migration in small, tested slices.
3. Project source snapshot and validation/cache reuse work for repeated project scans.
4. Plan-driven follow-ups for map partial refresh, GUI cleanup, SVG backgrounds, reporting, and 3D viewer refinement.

## Active Work

### 2026.7.2 Planning

- Treat Unified Source DOM completion as the primary `2026.7.2` release theme. Use
  `plans/UNIFIED_SOURCE_DOM_PLAN.md` as the ordered migration backlog and keep each implementation commit to one
  Raw/Blocks/Map/Structure/Validation/transaction slice.
- Keep SQL reporting improvements scoped to incremental follow-ups such as saved presets, filtering, summaries/charts, or
  direct database-export workflows after the active DOM slice is not at risk.
- Keep LiDAR/point-cloud processing as design/backlog work until the DOM migration has a stable Map/TH2 projection
  boundary; do not start heavy point-cloud implementation during the DOM completion push.

### 2026.7.1 Stabilization Notes

- Start the post-`v2026.6.9` cycle with architecture-aligned slices that reduce duplicate parsing, repeated project
  scanning, and source-transaction drift without broad parser or map-rendering rewrites.
- Prefer one focused implementation slice per commit, with matching tests and `python3 scripts/check_structure_constraints.py`
  before proposing a commit.
- Keep user-visible feature work aligned with `SPECIFICATION.md`; update the specification and `docs/USER_MANUAL.md` in the
  same change when behavior or workflows change.
- A first reporting POC opens Therion database export `.sql` files as read-only tabs, imports them into in-memory SQLite,
  exposes a guarded `SELECT` query editor above the result table, uses predefined centreline reports as query presets in
  the right sidebar, loads built-in preset SQL from `resources/sql_report_presets.json`, shows schema beside those presets,
  includes explorer and surveyor counts plus total depth in the overview preset, keeps the built-in preset set focused on
  11 production-ready reports, presents capped query results through a read-only table model, and exports the current
  result table as CSV from a SQL-specific toolbar download action with a project-specific timestamped default filename.
- A first `Outputs` project sidebar slice groups generated Therion exports by model, map/atlas, and database artifacts,
  keeps duplicate filenames distinct through project-relative paths and canonical-path identity, opens `.lox` and `.sql`
  internally where supported, sends `.pdf` outputs to the system default application, shows compact filenames while adding
  project-relative folder context only for duplicate names, refreshes after Therion runs finish, and shares project
  file-discovery traversal with project search instead of owning UI-side recursive scanning.
- Project source snapshot collection now also uses shared project file discovery for filesystem traversal and skip rules,
  so validation and structure scans inherit the same low-level project-tree discovery behavior without changing their
  source snapshot, cache, or project-index semantics.
- Therion runner status now treats stderr output-write warnings such as `warning -- error writing` as failed runs even
  when the process exits with code 0, so the status bar does not show a misleading `C: OK`.
- Tester feedback says application launch is slow while TH2 rendering and compilation feel fast; startup timing checkpoints
  are now emitted into troubleshooting logs so the next report can identify whether startup, bootstrap, session restore,
  document restore, project open, or first event-loop readiness is the bottleneck.
- Clean synthetic text-change notifications during session restore are logged as skipped and no longer request duplicate
  `DocumentChanged` project validation; dirty project text edits still trigger live validation.
- Troubleshooting logging no longer writes `THERION_STUDIO_ENABLE_LOG` when enabled from the UI preference; internal
  diagnostic timing helpers follow the active diagnostic handler so the 24-hour preference expiry is honored on restart,
  while explicit environment overrides remain available for developer launches.
- Feedback follow-up: the Welcome tab now exposes the existing searchable user manual directly, and opening the manual
  places keyboard focus in its search field across platforms.
- Map insertion onboarding now makes the no-scrap state explicit in `Selection`: the target selector stays hidden and the
  pending object message names the draft scrap that will be created.
- New raster background insertion keeps XTherion raster metadata anchored at the top edge while the inspector reports the
  corresponding model-space image position, such as `Y = -image height` for a new image anchored at zero.
- Compiler output now turns Therion source diagnostics such as `file.th2 [64]` into clickable links that open the source
  document on the reported line, compiler `error --` output is highlighted in red, and Therion warnings are highlighted
  in orange.
- Thconfig syntax validation now treats catalog wildcard options such as `export map -layout-xxx` as option families
  backed by known `layout` command options, so valid `-layout-*` export overrides no longer produce false
  unknown-option warnings while unknown layout suffixes and literal placeholder options still do.
- Thconfig/layout validation now treats `code ... endcode` bodies as raw backend code, so content lines that happen to
  start with non-layout Therion-looking words do not open nested blocks; parent-context layout commands before `endcode`
  report the active `code` block as unclosed, and closing directives validate against the parent block context so
  `endcode` no longer triggers unexpected-context warnings inside `layout`.
- Project source-reference resolution now treats quoted paths, leading `./` paths, and Windows-style backslash separators
  as equivalent to normal forward-slash relative paths, while validation warns on backslash path tokens and offers an
  `Apply Fix` that converts only the affected path token to portable `/` separators while previewing the full source line
  after the replacement.
- Validation results can now be exported as a single Markdown report from the Validation panel, including the current
  summary, grouped findings, source snippets, safe-fix previews, and a project-specific timestamped default filename for
  sharing or documentation.
- Project-level validation findings now render first under a `Project` group and selection shows their detail without
  trying to open the project folder as a source file.
- Draft release notes for `2026.7.1` are now available in `docs/releases/2026.7.1.md` with the SQL report viewer covered
  as one feature block, the Improvements/Bug Fixes sections condensed for release readability, and the default CMake
  application version bumped to `2026.7.1`; project config inference now prefers `thconfig`, `thconfig.thconfig`,
  `main.thconfig`, `index.thconfig`, then `<project_name>.thconfig` before falling back to a single unambiguous named
  config. Final release prep should still confirm packaging outputs and a short manual smoke pass before tagging.
- Project file create/rename/delete and external project file changes now invalidate shared project scan caches, refresh
  Structure, Validation, Outputs, and map-object projections, clear stale Compiler `Target Config` paths, and pass the
  resolved target config into project validation so Compiler, Structure, and Validation use the same project graph.
- 3D viewer PNG export now renders text overlays at the requested export resolution instead of scaling low-resolution
  canvas text textures, keeping labels, HUD, legends, and station labels sharper in 4K exports while preserving the
  same overlay layout as the interactive 3D viewer; saved PNG files are normalized to DPR 1.0 opaque RGB images so
  external viewers do not have to handle unnecessary alpha/retina metadata.
- Reporting follow-ups should stay incremental: decide whether to add saved report presets, per-report filters, result
  summaries/charts, or a direct Therion `export database` action before expanding the SQL viewer into a broader analysis
  workspace.

### Unified Source DOM / Transactions

- Use `plans/UNIFIED_SOURCE_DOM_PLAN.md` as the detailed slice queue.
- Raw completion prefix detection now uses shared logical token ranges for parsed cursor tokens while preserving the
  existing completion-character filtering for path and partial-token behavior; keep this covered in
  `TextEditorRawEditorQTests`.
- Raw context help command selection and the completion-popup required-argument fallback now resolve commands through
  logical offsets instead of physical-line rescans, and the synthetic input-path insertion helper now uses a shared
  logical document for its command check.
- Blocks canvas data-body scanning now uses shared logical commands when deciding where data rows end, so continuation
  rows after a data body do not get reparsed as standalone raw lines.
- Blocks details selection loading now reads selected logical commands from a source snapshot when populating read-only
  fields and option rows for continued commands.
- Blocks logical-line consumers now share a DOM-aware parsed-line helper that prefers cached
  `TherionSourceLogicalDocument` commands and source-document physical lines before falling back to legacy synthetic-line
  parsing.
- Blocks document outline data-body scanning now reuses `TherionSourceLogicalDocument::commandAtPhysicalLine()` for
  physical-row lookup, with regression coverage that comments inside a `data` body do not truncate the tracked range.
- Blocks toolbox auto-scope lookup now uses the shared logical source snapshot and a revision-keyed cache instead of
  reparsing physical lines, and regression coverage keeps the resolved insertion context anchored to the selected
  command's parent block.
- Blocks selection-details data-header readings-order chips now reuse already parsed logical tokens instead of
  retokenizing the joined readings-order string, preserving the same UI behavior with less local parsing.
- Blocks option-argument editors now reuse the shared command-option editor parser instead of tokenizing the value cell
  locally, so arity-aware splitting stays aligned with the rest of the command-editing stack.
- Blocks data-block dialog now builds one shared `TherionSourceDocument` snapshot for its scope and row scans instead of
  reparsing each row with `parseLine(...)`, and its data-header field parsing now reuses parsed tokens instead of
  tokenizing the joined column string again.
- Blocks delete executor now uses the same shared `TherionSourceDocument` snapshot for `data` scope scans and body-range
  detection instead of reparsing each scanned line independently.
- Blocks line-build service now reuses a single `TherionSourceDocument` snapshot for logical-line parsing instead of
  reparsing the selected line directly.
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
- Map canvas source-transaction test contexts now provide the same logical-source callback shape as production contexts,
  keeping partial-refresh regression coverage aligned with the shared logical-source path.
- Map partial-refresh regression coverage now verifies that vertex index entries point to live scene items for the
  refreshed line after one-line item replacement.
- Next implementation slice: pause DOM migration and switch to the next `2026.7.1` performance/UX item, or start a
  dedicated TH2 geometry projection design slice before touching rewrite planners.
- Keep Therion namespace/reference changes behind `docs/THERION_COMPATIBILITY.md` coverage, especially
  `object@child.parent` qualified-reference order.
- Keep source transaction ownership work incremental: one caller or workflow per commit, with explicit result handling,
  undo label, revision behavior, projection invalidation, dirty-state behavior, and selection/cursor restoration.
- Source transaction diagnostics now split undo timing into command creation, `QUndoStack::push`, and guard overhead so
  map vertex-move logs can identify whether remaining latency is snapshot allocation or undo-stack insertion.
- Map undo-stack index changes now update the command surface once through the undo-owner handler instead of also using a
  duplicate direct command-surface connection during every map undo push.
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
- Project validation controller now suppresses scanner results from superseded request serials before they reach the UI,
  so queued automatic refreshes cannot briefly republish stale diagnostics while a newer validation request is pending.
- Project validation scanner now checks superseding request serials during collection/local-validation boundaries and
  suppresses superseded worker results instead of finishing and emitting stale scans.
- Project validation now passes scanner cancellation into project-index scans, allowing superseded workers to abort before
  completing map/join/station/duplicate diagnostic passes and preventing canceled index snapshots from entering the cache.
- The latest validation log confirms superseded project-index scans now stop earlier, while alternating automatic triggers
  can still thrash the single-entry source/index snapshot cache.
- Project scan caching now retains a small window of recent source and project-index snapshots so alternating automatic
  validation requests can reuse recently displaced scan results instead of rebuilding the full project index.
- Follow-up validation logs confirmed alternating automatic save/change/file-watch triggers now usually hit source/index
  snapshot caches and avoid repeated full project-index rebuilds.
- Validation UI diagnostics now split problem-tree timing into expand, resize, selection/detail, and open-navigation
  buckets so expensive `fix-applied` reveal paths can distinguish tree work from editor navigation.
- Next validation/UI slice: gather a fresh diagnostic log for `fix-applied` reveal paths and only optimize the Validation
  tree if `tree_expand_ms` or `tree_resize_ms` is the measured bottleneck; do not reintroduce scanner-local file traversal,
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
- Line partial refresh now preflights that the target geometry item group still has a live primary scene index before
  removing scene items, falling back to the pending full refresh when scene metadata is incomplete.
- Point geometry moves now use the same one-feature partial-refresh item replacement path as line moves when source bounds
  stay projection-compatible, avoiding a full map scene refresh after simple point drags.
- Point partial-refresh selection restore now captures the explicit canvas context so deferred callbacks do not depend on
  the lifetime of the temporary edit controller used by production map input.
- Touch updates during an active primary map interaction are now accepted as suppressed primary updates instead of being
  passed through as pan/scroll candidates while the item drag path owns the gesture.
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
- Keep Linux strict-warning builds green by updating aggregate initializers when project scan/source structs gain fields.
- Keep app-library and editor-test support source ownership aligned with static-link boundaries; shared UI components such
  as `InspectorPanel` must be linked through every static library that compiles consumers of those symbols, not only
  through the main executable or a single test runner.
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
- Current stabilization slice adds PNG image export from the current 3D viewport, explicit export resolution presets,
  and black/white scene background selection.
- Next implementation slice should add or identify a real Therion-exported `.lox` fixture with terrain surface chunks,
  or add debug/log load/render statistics before renderer refactoring.
- Keep the viewer read-only and keep `.lox` loading/model/statistics in core.
- Decide whether to add Therion `.3d` model support to the 3D viewer or keep `.3d` outputs hidden from the Outputs pane.

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
- Add a manual `Help -> Check for Updates...` workflow only after deciding how to handle networking without destabilizing
  AppImage/Linux packaging; do not add Qt Network or automatic startup checks until the dependency/deployment impact is
  explicitly planned.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Bounded `.xvi` cache policy for very large projects.
- Station marker/label priority ranking follow-up: tune automatic decluttering if dense projects hide important stations.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX follow-up for hardware-specific pressure, hover, and tablet-driver behavior.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, stable raster position anchors during scale/rotation, XTherion rewrite caveats, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
