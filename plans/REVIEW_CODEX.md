# Codex Repository Review

Date: 2026-07-13

Scope: repository-wide static architecture and maintainability review against `SPECIFICATION.md`, `ARCHITECTURE.md`,
and `AGENTS.md`, with targeted inspection of source integrity, asynchronous workflows, Map rendering and editing,
project scanning, persistence, localization, cross-platform behavior, and test infrastructure.

This document is the active review record. It replaces the 2026-06-02 review and its 2026-07-12 migration status
update. Completed migration history belongs in `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md`; implementation work should
be selected from the current findings below and tracked briefly in `WORKLOG.md`.

## Executive Summary

The repository has a substantially stronger foundation than the previous review described. The unified lossless
source model is implemented, direct parser entry points and Map source mutations are guarded, project validation is
asynchronous and cancellation-aware, production dependencies are explicitly injected into the main editor shells,
and the stable editor directory layout is enforced. No confirmed P0 source-corruption or security defect was found in
this review.

The remaining risks have shifted from parser convergence to runtime coordination and ownership. The Structure/Outputs
supersession finding identified by this review was resolved on 2026-07-13; the remaining open risks are:

1. SQL report import and user queries can block the UI thread without cancellation;
2. Map refresh still performs a synchronous full scene rebuild and repeatedly copies cached projections by value;
3. recursive project watcher setup runs on the UI thread;
4. Map style/background resource caches remain hidden static state in presentation code;
5. localization checks do not detect visible strings that never enter Qt translation extraction.

These are focused follow-ups, not reasons to reopen the DOM migration or start a broad renderer rewrite. The safest
sequence is to fix worker result generations and SQL responsiveness, then move Map resource ownership and refresh work
behind explicit revision-keyed services.

## Review Method and Verification

The review used:

- the complete architecture and agent guardrails plus the functional/acceptance sections of `SPECIFICATION.md`;
- repository structure and translation-unit size scans over 507 production C++ source/header files;
- targeted source inspection of source transactions, Map refresh and background loading, project scan/cache workflows,
  SQL reports, session/settings ownership, localization construction, process execution, and test runners;
- `python3 scripts/check_structure_constraints.py` — passed;
- `python3 scripts/check_localization.py` — passed for shipped Czech and Slovak catalogs, with the blind spot described
  in P1-7;
- Release CTest inventory — 67 tests: 48 `unit`, 19 `ui`;
- `ctest --test-dir build-release -L unit --output-on-failure --timeout 120 -j 4` — 47 of 48 tests passed;
  `TherionCoreQTests` failed because `ThreeDViewerLoxLoaderTest::loadsSampleLoxScene()` required a missing optional
  sample file.

This was a static review plus focused automated verification, not a claim that every specification acceptance
criterion was manually exercised on all three platforms. Hardware-specific stylus/Sidecar behavior and packaging
artifacts still require their dedicated verification paths.

## Architecture Baseline That Is Working

The following previous high-priority concerns are now closed or materially reduced:

- `TherionSourceText`, `TherionSourceDocument`, `TherionSourceLogicalDocument`, `TherionSourceSnapshotCache`, and
  `Th2GeometryProjection` form the shared source/projection baseline.
- `scripts/check_structure_constraints.py` rejects unapproved direct parser calls, Map source-mutation bypasses,
  directory regressions, missing CMake source wiring, and growth in selected shell translation units.
- Map mutations route through `TextEditorSourceTransactionController` and the atomic snapshot helpers. Revision checks,
  dirty-state updates, projection policies, and focused undo/redo coverage are present.
- The earlier multi-attempt fixed-delay Map selection-retry finding is no longer supported by the current code. Current
  selection recovery uses next-event-loop callbacks, callback QObject lifetime guards, and generation checks. New work
  should still prefer explicit scene-generation completion over additional timer sequencing.
- Project validation runs through `QtConcurrent`, suppresses superseded requests, shares explicit scan-cache ownership,
  and reuses source/logical/index projections.
- Therion execution uses `QProcess` with a program plus argument list, streams stdout/stderr asynchronously, rejects a
  parallel run deterministically, and centralizes platform search paths.
- Core/domain code does not include QtWidgets or QGraphics presentation types.
- Shipped localization catalogs currently contain no unfinished Czech or Slovak entries according to the repository
  localization checker.

## Priority Findings

### P0

No confirmed P0 finding was identified. Source rewrite and transaction work remains high-risk by nature, but the
current guardrails and focused tests are evidence against treating it as an active release blocker without a concrete
regression.

### P1-1 — The Core Test Runner Is Not Hermetic — Resolved 2026-07-13

Resolution:

- mandatory loader coverage uses the committed `tests/fixtures/three_d_viewer/1302.lox` fixture and explicit CMake
  fixture root;
- ignored real-project files run only through opt-in `ThreeDViewerLoxCorpusTest` and the `corpus` CTest label;
- every known optional fixture is an independent row with a missing-file skip;
- mandatory `ThreeDViewerLoxCorpusPolicyTest` covers unrelated, partial, relative-root, and absolute-root resolution.

Verification:

- Release `unit` label passes without corpus discovery and with the local partial/complete corpus present;
- complete optional corpus passes all ten known rows;
- one-file optional corpus passes one row and skips nine independently;
- fixture provenance and opt-in invocation are documented in `tests/fixtures/three_d_viewer/README.md` and
  `docs/BUILDING.md`.

### P1-2 — SQL Import and Queries Can Block the UI Indefinitely

Status: resolved 2026-07-13.

Evidence:

- `TherionSqlReportWorkerSession` now dispatches imports and queries to the connection-owning worker thread, so these
  operations no longer freeze the UI event loop.
- `TherionSqlReportTab` suppresses results whose request ID or source generation has been superseded and disconnects UI
  publication before asynchronous teardown.
- `TherionSqlReportExecutionControl` synchronizes native connection lifetime with cross-thread interruption and keeps
  cancellation, request generation, and monotonic deadline state atomic.
- The SQLite progress handler enforces a ten-second query deadline. Newer requests and shutdown call
  `sqlite3_interrupt()` immediately, while worker results distinguish cancellation from timeout.
- Recursive-query tests prove timeout, supersession cancellation, recovery on the same worker, and bounded interrupted
  teardown.

The report-specific cleanup is complete: preset/settings and CSV IO ownership now sit behind explicit injected
adapters. Cross-platform package jobs remain the final verification of the S3A platform dependency wiring.

### P1-3 — Structure and Outputs Publish Superseded Worker Results

Status: resolved 2026-07-13.

Evidence:

- `src/app/ProjectStructureScanner.cpp:38-46` replaces the pending request, and `:54-63` only records that another scan
  is queued while the current worker runs.
- `src/app/ProjectStructureScanner.cpp:109-117` unconditionally emits the completed old result before starting the queued
  replacement.
- `src/app/ProjectOutputsScanner.cpp:119-148` and `:151-159` use the same pattern.
- `src/app/MainWindowStructureBrowser.cpp:650-665` checks only the project root, not the latest requested generation.
  A stale result for the same project can therefore replace newer in-memory Structure state temporarily.
- Project validation already has the safer request-serial/supersession pattern and can be used as the local reference.

Impact:

- Rapid edits, target-config changes, or project mutations can make Structure/Outputs briefly move backward before the
  next result arrives.
- Slow scans waste work and make behavior timing-dependent.

Recommendation:

- Add a monotonically increasing request serial and suppress results older than the latest request.
- Pass cancellation/supersession checks into Structure indexing and file discovery where practical.
- Add tests where request B arrives while request A is running and assert that A is never published.

Resolution:

- Structure and Outputs now assign request serials when requests are accepted, publish only the latest result, and start
  only the latest queued replacement.
- Presentation handlers reject stale serials and mismatched normalized project roots before changing models, messages,
  or console output.
- Shared app/service QTests cover same-root and changed-root replacement, stale errors, latest-pending execution, empty
  Outputs state, and teardown before completion delivery; the aggregate runner passes repeated release execution.

### P1-4 — Map Refresh Still Has a Synchronous Full-Rebuild Boundary

Evidence:

- `src/app/text_editor/map_editor/MapEditorSceneRefreshController.cpp:198-245` captures viewport state and clears the
  scene before rebuilding it.
- `:247-319` collects logical commands and geometry and renders the complete scene synchronously on the UI thread.
- `src/app/text_editor/map_editor/MapEditorTabSourceEditWorkflow.cpp:78-124` caches by document revision, but
  `logicalCommandsForCurrentDocument()` and `geometryProjectionForCurrentDocument()` return complete projections by
  value.
- Selection and inspector paths invoke the value-returning projection callbacks repeatedly; partial point/line refresh
  exists, but many text, inspector, area, and background paths still fall back to the full rebuild.

Impact:

- Cache hits avoid reparsing but not repeated projection handoffs, derived feature collection, item destruction, and
  item recreation.
- Full scene replacement increases UI latency, battery use, selection-restoration complexity, and stale raw-item risk.

Recommendation:

- Expose immutable revision-keyed projection handles/references instead of repeated value-returning callbacks.
- Separate worker-safe projection construction from main-thread scene application.
- Give every scene projection a generation ID and make selection/navigation restoration target that generation once.
- Continue widening delta refresh only behind `MapEditorLargeScenePerformanceSmokeTest`; do not combine this with a broad
  style-renderer rewrite.

### P1-5 — Recursive Project Watch Setup Runs on the UI Thread

Evidence:

- `src/app/MainWindowProjectFileWatcher.cpp:86-116` recursively walks every project directory and collects every Therion
  source file.
- `:119-148` rebuilds all directory/file watches and signatures synchronously.
- `:168-217` may rebuild the full watcher inventory again after a directory change.

Impact:

- Opening or mutating a large/deep project can block the main thread.
- Watching every directory and source file can hit platform watcher limits and has no explicit partial-failure reporting.
- This duplicates project traversal concerns inside the `MainWindow` presentation shell.

Recommendation:

- Move project watch inventory/discovery behind an application/infrastructure service and build it off the UI thread.
- Apply generation-keyed watcher deltas on the main thread and report paths that could not be watched.
- Measure large-tree startup and mutation behavior before choosing between recursive watches, bounded directory watches,
  or a hybrid periodic reconciliation policy.

Progress (2026-07-13): W1 extracted the deterministic, presentation-free `ProjectFileWatchInventoryCollector` with
focused coverage for skipped directories, symlinks, outside-root paths, signatures, and discovery errors. W2 adds the
generation-keyed `ProjectFileWatchInventoryService`, so collection is now off the GUI thread and stale results are
suppressed. P1-5 remains open until W3 applies completed inventories as GUI-thread watcher deltas.

### P1-6 — Map Catalog and Background Caches Are Hidden Static Presentation State

Evidence:

- `src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp:991-1020` reads bundled resources, environment settings,
  user application-data overrides, and stores the result in a function-static catalog.
- `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp:2166` loads that global catalog from the renderer path instead
  of receiving it from composition.
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp:265-299` owns an unbounded function-static XVI document
  cache in a UI translation unit. Every lookup still reads and hashes the whole file before obtaining a cache hit.
- Bounded raster caches exist in `MapEditorRasterBackgroundImage.cpp`, but they are also process-global mutable state
  rather than explicit dependencies.

Impact:

- Cache lifetime, memory bounds, invalidation, and test isolation are implicit.
- The XVI cache grows for the life of the process and does not eliminate repeated file I/O/hashing.
- UI-side resource/settings access conflicts directly with the composition and static-state guardrails.

Recommendation:

- Compose one style-catalog provider and one bounded background asset cache at application startup.
- Inject immutable catalog data into Map rendering/preview contexts.
- Key background entries by canonical identity plus size/mtime or content revision, use a memory-cost-aware LRU, and make
  invalidation explicit on file changes.
- Keep cache loading/parsing off the UI thread and add deterministic fake caches for tests.

### P1-7 — Localization Verification Misses Unextractable Visible Strings

Evidence:

- `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp:1808-1826` returns a complete visible Map help page through
  `QStringLiteral`, so it cannot be translated.
- `src/app/text_editor/raw_editor/RawEditorCommandMetadataLoader.cpp:317-332` builds visible `Option`, `Description`,
  `Value Arity`, and `Accepted Values` labels as literals; raw internal arity tokens are also exposed directly.
- `src/app/ProjectTemplateService.cpp:63-151` and `:162-206` return user-facing project-creation failures as untranslated
  literals.
- `scripts/check_localization.py` validates entries already present in `.ts` files, but does not prove that all visible
  source strings were passed through Qt translation APIs or that catalogs were freshly extracted.

Impact:

- Czech and Slovak builds can pass the localization gate while still displaying English-only core workflow text.
- Stale `.ts` entries can mask source strings that stopped participating in extraction.

Recommendation:

- Route visible service errors through a translatable presenter/error-code mapping or translated service context.
- Translate help labels/templates while leaving Therion syntax and source-authored catalog text unchanged.
- Add a CI extraction audit using `lupdate` into a temporary catalog and compare source keys/locations, plus a focused
  denylist scan for known visible setter/help/error construction patterns.

## Secondary Findings

### P2-1 — Presentation Components Still Own External State and File I/O

- The report-specific part is resolved: `TherionSqlReportTab` receives its preset/settings adapter and CSV file exporter
  from `MainWindow`, retains only dialog/presentation work, and has focused CSV parity and injected-fake coverage.
- `MainWindowHelpDialog.cpp:84-99` reads application settings directly while resolving manual language.
- `MapEditorBackgroundLayers.cpp` combines dialogs, file loading, parsing, asynchronous image work, session state,
  graphics-item updates, and source transactions in one presentation translation unit.

Move persistence and file workflows behind injected services. Widgets should select intent and present results, not own
storage adapters.

### P2-2 — Map Undo Stores Full Document Snapshots

`TextEditorSourceSnapshotCommand` stores complete before/after strings for every command
(`src/app/text_editor/TextEditorSourceTransactionController.cpp:95-170`). Map undo is bounded to 200 commands
(`src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp:66-97`), but memory still scales with document size times
history depth even when the initial edit is a narrow `TherionSourceTextEdit`.

Keep the transaction contract, but evaluate reversible range edits or shared immutable snapshots with measured memory
budgets. Preserve full snapshots as a safe fallback for structurally complex rewrites.

### P2-3 — Large Files Still Mix Responsibilities

Eight production files exceed 2,000 lines. The highest-risk examples are:

- `MapEditorBackgroundLayers.cpp` — 3,904 lines;
- `MapEditorSceneRenderer.cpp` — 3,476 lines;
- `TherionDocumentEditor.cpp` — 3,464 lines;
- `MapEditorViewportInputController.cpp` — 2,625 lines;
- `MapEditorCanvasEditController.cpp` — 2,420 lines;
- `ProjectStructureIndex.cpp` — 2,322 lines;
- `ThreeDViewerViewportItem.cpp` — 2,230 lines;
- `MainWindow.cpp` — 2,201 lines.

Split only by proven responsibility boundaries. Highest leverage: background asset/cache/metadata/source adapter,
scene geometry/style/overlay rendering, document edit planners by object family, and project graph scan/namespace
resolution. Do not use line-count-only mechanical splits.

### P2-4 — Test Architecture Migration Is Incomplete

The repository has 130 test `.cpp` files and 64 explicit `main()` entry points. Several explicit mains are valid
aggregate QTest runners or isolated UI/crash boundaries, but many legacy hand-rolled executables and very large test
files remain. Five test translation units exceed 2,000 lines, led by `TherionDocumentEditorTest.cpp` and
`MapEditorDragUndoRedoSmokeTest.cpp` above 3,500 lines.

Continue incremental QTest migration when touching a test. Split large smoke executables by lifecycle/runtime boundary,
not one executable per tiny class, and retain isolation for genuinely flaky, process-backed, or crash-containment cases.

### P2-5 — Infrastructure Placement Does Not Fully Match the Documented Layers

`SessionSettingsStore` is a real `QSettings` adapter in `src/core/SessionStore.*`, and `CommandCatalogStore::fromFile()`
performs file I/O in `src/core/CommandCatalogStore.cpp`. The interfaces/value parsing are core-appropriate, but concrete
settings and file adapters belong in infrastructure/platform composition according to `ARCHITECTURE.md`.

Move concrete adapters only as a focused no-behavior-change refactor; avoid churning stable domain loaders merely to
obtain a visually pure directory tree.

### P2-6 — Active Planning Documentation Contains Completed History

`WORKLOG.md` remains dominated by completed implementation notes despite its “active planning only” header. The known
stale post-DOM Map segfault and fixed-delay selection-retry follow-ups were removed with this review, but the remaining
release history still makes current priorities harder to identify than necessary.

Condense current/open work into short sections and move release history to release notes or archive plans. The current
review should remain the detailed architecture risk register.

## Minor Cleanup Candidates

These are not priority work by themselves:

- `BlockEditorConfigureController::configureBlock()` still accepts and explicitly ignores `showCommandHelpOnly`
  (`src/app/text_editor/block_editor/BlockEditorConfigureController.cpp:27-45`). Remove the parameter or implement a real
  semantic caller when the area is next touched.
- `BlockEditorApplyExecutor` and `BlockEditorApplyStateController` retain Apply-oriented names after the UI moved to
  auto-commit. Rename only in a focused touched-area refactor.
- Icon rendering remains duplicated in `LucideIconFactory`, `BlockEditorCanvasItem`, Structure, and Map inspector code.
  Consolidate into one bounded DPR-aware factory when doing UI cleanup, without coupling it to source-model work.

## Recommended Delivery Sequence

1. **Restore a hermetic test baseline — completed**
   - fix the optional `.lox` fixture contract;
   - keep `unit`, `ui`, corpus, performance, and packaging responsibilities explicitly labeled.
2. **Make asynchronous results monotonic — completed 2026-07-13**
   - add request serial suppression to Structure and Outputs;
   - add focused supersession tests.
3. **Protect UI responsiveness**
   - move SQL import/query execution to a worker-owned connection with cancellation;
   - move project watcher inventory traversal out of `MainWindow`.
4. **Close localization extraction gaps**
   - translate current literals;
   - add a fresh-extraction CI audit.
5. **Make Map resources explicit**
   - inject the style catalog;
   - replace static background caches with bounded services.
6. **Reduce Map rebuild cost incrementally**
   - immutable projection handles first;
   - scene generation/completion next;
   - measured delta refresh expansion last.
7. **Continue opportunistic structure cleanup**
   - split large files by responsibility;
   - migrate touched tests and infrastructure adapters without mixing these changes with feature work.

## Implementation Plan Map

Concrete execution order, smaller-model operating rules, cross-plan dependencies, and the verified scope status of every
active plan are maintained in `plans/REVIEW_IMPLEMENTATION_PLAN.md`.

Focused implementation plans:

- P1-1 / P2-4: `plans/TEST_HERMETICITY_PLAN.md`;
- P1-3 and P1-5: `plans/PROJECT_ASYNC_COORDINATION_PLAN.md`;
- P1-2 / report-specific P2-1: `plans/SQL_REPORT_ASYNC_PLAN.md`;
- P1-7: `plans/LOCALIZATION_EXTRACTION_PLAN.md`;
- P1-6 / P2-2: `plans/MAP_RUNTIME_OWNERSHIP_PLAN.md`;
- P1-4: revised `plans/MAP_PARTIAL_REFRESH_PLAN.md`.

These plans are intentionally independent commit chains. Completing a design seam does not close a finding until its
production caller, focused tests, and exit gate are complete.

## Verification Gates for Follow-up Work

- Source/parser changes: round-trip `.th`, `.th2`, and config fixtures; CRLF/mixed line endings; comments/unknown
  directives; encoding; source ranges; undo/redo.
- Map refresh/cache changes: `MapEditorLargeScenePerformanceSmokeTest`, repeated parallel UI smoke runs, selection and
  Bezier/area overlap cases, background visibility/load/transform, and stale generation teardown.
- Project scanner changes: superseded same-root requests, target-config changes, in-memory unsaved text, external file
  changes, cancellation, and cache invalidation.
- SQL changes: large import, malformed export rollback, long-running query cancellation, tab close during work, stale
  result suppression, and CSV output parity.
- Localization changes: fresh `lupdate` extraction, Czech/Slovak completion and placeholder checks, plus runtime smoke of
  Map help, project-template errors, and Raw option help.
- Every implementation slice: `python3 scripts/check_structure_constraints.py`, focused tests, and `git diff --check`.

## Final Recommendation

Keep the unified source architecture closed and stable. With Structure/Outputs publication now monotonic, the next
release-quality gains come from removing UI-thread SQL/project traversal and moving hidden Map resource state to
explicit bounded services. Only after those boundaries are in place should Map partial refresh be widened.
