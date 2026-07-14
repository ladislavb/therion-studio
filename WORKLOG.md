# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. `2026.7.2` post-DOM stabilization and next feature selection.
2. Complete watcher W4 in `PROJECT_ASYNC_COORDINATION_PLAN.md`: verify large-tree responsiveness, root replacement,
   directory mutations, and cross-platform watcher behavior. SQL S1-S5 and watcher W1-W3 are complete.
3. Real-project smoke testing for Raw, Blocks, Map, Validation, Structure, and Compiler navigation after DOM closure.
4. Plan-driven follow-ups for map partial refresh, validation/cache tuning, GUI cleanup, SVG backgrounds, reporting, LiDAR design, and 3D viewer refinement, including palette-regression coverage for inspector controls.
5. Verify the Windows unit-test stabilization and keep session restore working for accessible projects in standard macOS user folders; unit CTest cases now identify individual QTest suites without multiplying test binaries. The Windows workflow reruns failed suites directly with verbose QTest output written to and explicitly printed from a file; the diagnostic path has a local PowerShell fixture check. Current Windows portability follow-up covers CSV newline output and platform-correct filesystem test fixtures, including platform-specific link assertions. On failures, verify production code before considering any test change, which requires explicit approval.

## Active Work

### 2026.7.2 Planning

- Treat Unified Source DOM implementation as closed for `2026.7.2`; the completed M0-M9 plan is archived at
  `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md`. Future parser/source-model work should treat the DOM as the current
  architecture and extend it through focused regressions rather than reviving the migration queue.
- Raw source workspaces now expose an explicit `Format Document` toolbar action with a `text-quote` icon. It uses the shared source document
  structure to normalize leading indentation to literal tabs in one undo step while preserving code bodies, blank rows,
  line endings, and encoding; formatting remains opt-in rather than an opening/save side effect.
- The SQL report tab now receives an explicitly composed worker session, accepts file loads without blocking on import,
  keeps import/query requests generation-keyed, suppresses stale schema/table results, presents coherent busy/error state,
  and disconnects UI publication before asynchronous worker teardown. The report database has migrated from QSQLITE to
  a directly owned platform SQLite handle while preserving import/query/schema/value behavior. Superseded requests and
  shutdown now interrupt active SQLite work, recursive queries have a ten-second progress-handler deadline, cancellation
  and timeout are distinct results, and the same worker recovers for later queries. `MainWindow` now composes the
  settings-backed custom-preset store and CSV file exporter; the tab keeps only preset/dialog presentation while
  deterministic CSV serialization and file IO are focused services with parity coverage.
- Keep LiDAR/point-cloud processing as design/backlog work until the Map/TH2 projection boundary has a concrete import,
  registration, and 2D projection design.

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
- Every in-project source-text mutation, including undo back to the saved text, requests a `DocumentChanged` project
  validation so stale diagnostics cannot remain after the document becomes clean; the controller still honors the
  automatic-validation setting.
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

### Post-DOM Stabilization

- Keep the shared source model architecture guarded by `scripts/check_structure_constraints.py`; new direct
  `TherionDocumentParser` production calls should remain limited to the documented core/synthetic exception list.
- Run real-project smoke passes before broadening source-model behavior again: Raw completion/help/validation, Blocks
  nested/data/continued commands, Map selection/inspector/create/delete/move/split/backgrounds, Structure, Validation,
  and Compiler diagnostic navigation.
- Keep further parser/source-model changes incremental and regression-backed; do not revive the archived DOM migration
  queue for ordinary feature work.

### Validation And Catalog Metadata

- Treat full-project validation as an explicit/manual workflow by default; use the Settings toggle only when projects are
  small enough or the user wants live full-project diagnostics.
- Use `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` only for future validation/cache follow-ups; Structure and
  Validation already share snapshot-compatible collection/index input paths and explicit cache ownership.
- Project station-reference validation now applies a scrap's `-station-names <prefix> <suffix>` transform before
  resolving the complete station token against centerline data in both indexed and unindexed TH2 validation paths,
  including quoted empty prefix/suffix values; unindexed qualified references are resolved against the cached project
  station index, including relative namespaces; map-editor project diagnostics are retained while Visual mode is
  active so they become visible when the affected document is opened in Raw mode, and map-editor changes (including
  undo back to a clean document) participate in debounced live project validation from the in-memory source snapshot,
  with focused QTest coverage for valid, missing, and invalid-namespace references.
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

- Map scene scrolling now expands around visible background layer bounds while keeping the base map/source projection fixed, so a moved, scaled, or rotated raster/SVG/XVI layer can be panned to every edge instead of being cut off at the original canvas rectangle; asynchronously loaded backgrounds also suppress stale empty-map guides once they become visible.
- Map `Fit` now falls back to the fixed canvas when a document has no geometry; backgrounds are included only by the explicit `Fit With Background` action, avoiding a scrollbar-resize loop in empty maps with asynchronously loaded backgrounds.
- Map primary clicks now keep highlighted-path pending metadata for overlapping line hits while treating an actual area-fill hit as authoritative near shared borders, preserving both Bezier handle activation and high-zoom area selection.
- Map file loading now cancels the debounced source-edit refresh before performing its explicit scene refresh, preventing a delayed duplicate rebuild from invalidating freshly exposed scene items during early interaction; cross-platform smoke fixtures reacquire scene items after event-loop boundaries and use native temporary project roots.
- Background session state now only supplements layers declared in the current TH2 metadata; it can no longer silently restore a session-only drawing reference into an empty or unrelated TH2 document.
- Map point insertion now captures the original scene rectangle and exact scrollbar values before its source transaction, then reapplies them after every deferred projection and selection update; the values therefore retain their original scene-coordinate meaning and avoid both the old large remap jump and high-zoom rounding drift. Transitional viewport repaints remain suppressed until the preserved state is restored. The interactive drag/undo smoke test covers the regression.
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

- Treat `plans/REVIEW_CODEX.md` (2026-07-13) as the current architecture risk register and
  `plans/REVIEW_IMPLEMENTATION_PLAN.md` as its executable queue. Use exactly one numbered focused-plan slice per
  implementation turn.
- Use QTest for new C++ tests while keeping CTest as the runner.
- Keep `tests/core/` and `TherionCoreQTests` as the baseline pattern for small core-only QTest cases.
- Keep app/service/editor aggregate QTest runners grouped by dependency/runtime boundary rather than creating one
  executable per tiny class.
- Continue migrating touched hand-rolled tests to QTest where the dependency/runtime boundary is already clear.
- Keep `python3 scripts/check_structure_constraints.py` green and preserve guardrails against map-editor source mutation
  bypasses.
- Keep mandatory `.lox` coverage on the committed fixture root, optional real-project rows behind the `corpus` label,
  and corpus path-policy regressions inside the core aggregate runner.
- Keep Linux/Windows CI and package Qt runtime module lists aligned with QML inspector imports.
- Keep Linux strict-warning builds green by updating aggregate initializers when project scan/source structs gain fields.
- Keep app-library and editor-test support source ownership aligned with static-link boundaries; shared UI components such
  as `InspectorPanel` must be linked through every static library that compiles consumers of those symbols, not only
  through the main executable or a single test runner.
- Keep UI smoke tests deterministic across platform event-loop timing differences.
- When touching source-driven map scene refresh, synchronize tests through the explicit refresh-completion signal and repeat
  `MapEditorDragUndoRedoSmokeTest` to guard delayed refresh selection restoration for cursor-derived line/area ownership
  such as `endline` and `endarea`.
- Keep explicit user confirmation before every `git commit`.

### UI Cleanup

- Follow `plans/GUI_CLEANUP.md` for slice order.
- Next implementation slice should be one style extraction or repeated-metric cleanup, not a broad UI rewrite.
- Keep style policy, UI construction, presentation contracts, and source/model logic separated.
- Do not combine GUI cleanup with source-model, parser, validation, file IO, or process execution changes.

### SVG Backgrounds

- Follow `plans/SVG_BACKGROUND_PLAN.md` for remaining SVG background work.
- Invalid or missing restored SVG layers now report their filename in the Map status while valid sibling background
  layers continue loading; Gamma and Fit With Background are verified baseline behavior.
- Preserve Mapiah `format=svg` semantics and existing background source transaction paths.
- Keep general background cache/provider work in `plans/MAP_RUNTIME_OWNERSHIP_PLAN.md`.

### 3D Viewer

- Follow `plans/3D_VIEWER_PLAN.md` for remaining work.
- PNG export, export resolution presets, black/white scene background, Scene Settings, and Outputs `.lox` opening are
  implemented baseline behavior.
- Add optional real terrain/surface-bitmap corpus coverage only with per-fixture skip semantics; then add debug/log
  load/render statistics before renderer refactoring.
- Keep the viewer read-only and keep `.lox` loading/model/statistics in core.
- Decide whether to add Therion `.3d` model support to the 3D viewer or keep `.3d` outputs hidden from the Outputs pane.

## Blocked / Needs Input

- Old Therion/Metapost crash fixture: parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Continue the Map runtime-ownership plan after explicit style-catalog composition: add the immutable projection handle,
  scene generation, area dependency gate, then measured partial-refresh widening.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Broaden retained project validation cache coverage only if fresh logs show repeated scans still rebuilding unchanged
  source or project-index projections.
- Restore automatic full-project validation as the recommended/default mode only after live diagnostics are incremental,
  cancellable, and UI-cheap for nested projects.
- Add a manual `Help -> Check for Updates...` workflow only after deciding how to handle networking without destabilizing
  AppImage/Linux packaging; do not add Qt Network or automatic startup checks until the dependency/deployment impact is
  explicitly planned.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Replace static XVI/raster/SVG cache ownership through `plans/MAP_RUNTIME_OWNERSHIP_PLAN.md`, beginning with a bounded
  typed cache policy and explicit invalidation tests.
- Station marker/label priority ranking follow-up: tune automatic decluttering if dense projects hide important stations.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX follow-up for hardware-specific pressure, hover, and tablet-driver behavior.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, stable raster position anchors during scale/rotation, XTherion rewrite caveats, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
