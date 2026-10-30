# Therion Studio Architecture

This document records the intended architecture of Therion Studio. It is a design guardrail, not a product specification and not a task backlog.

The product source of truth remains `SPECIFICATION.md`. The active short-form implementation roadmap remains `WORKLOG.md`, with detailed working plans kept under `plans/`. The optional local project history and private GitHub backup design is described in [Project Versioning and Backup Plan](plans/PROJECT_VERSIONING_PLAN.md). Settings exposes default-off project opt-in, manual and timed checkpoints, history listing/deletion, and root project ignore-file editing. The isolated Git store never changes the project's Git metadata. The right sidebar's File tab provides project-level backup navigation and restoration. Pre-compilation checkpoints and remote synchronization remain pending. When architecture, implementation, and specification diverge, resolve the divergence explicitly instead of silently choosing one.

Project restoration uses `ProjectVersioningStore` for locked safety capture,
strict snapshot/path validation, staging, byte-preserving atomic file writes,
rollback and append-only restore records. A durable restore marker identifies the
safety checkpoint after interruption; normal capture is blocked until recovery.
`ProjectVersioningNavigationService` owns asynchronous history/restore operations;
Per-document `ProjectVersioningToolbar` panels in `DocumentFileInspector` present
checkpoint status and choices in the right File sidebar and emit requests.
All panels share the window's navigation service.
Settings uses separate local-history, GitHub and ignore-file pages.
`GitHubRepositoryWidget` persists the format version, owner, repository
name and upload/close-prompt opt-ins in the project root's `.therion-studio-github.json` using guarded atomic
writes. Credentials remain managed by GitHub CLI/system keyring. Loading project
metadata never performs authentication or network requests.
`MainWindowGitHubBackup.cpp` coordinates explicit save/commit-message/upload and
opt-in close prompts. Snapshot/network work runs outside the GUI thread behind
an application-modal progress surface. `ProjectGitHubBackup` uses a separate
application-owned store for upload snapshots and remote-chain state, never the
project's `.git` or the local restore history. A successful upload requires a
verified private target and remote ref confirmation; foreign changes are rejected.
`MainWindowProjectVersioning.cpp` coordinates existing save-all and document
reload/close workflows with an application-modal progress surface and a result
comparison. Timed checkpoints pause in this window during restoration. External
editors and other application instances are not filesystem-locked; concurrent
changes to affected files are checked and rejected rather than intentionally
overwritten. Links, ignored-file collisions and file/directory transitions are
explicitly unsupported in this restoration slice.

Therion language compatibility rules that affect parsing, namespaces, references, source preservation, validation, and project indexing are maintained in `docs/THERION_COMPATIBILITY.md`. Architecture and implementation changes that touch Therion semantics should align with that document or update it with focused verification.

## Architectural Goals

- Preserve Therion source files losslessly unless a user explicitly applies a source edit.
- Keep Raw, Blocks, Map, Structure, Validation, completion, highlighting, help, and compiler workflows as projections of shared source data rather than independent reparsers.
- Keep UI shells thin and move parsing, indexing, file IO, process execution, source mutation, and reusable workflow rules into focused services or core types.
- Keep the UI responsive by making expensive parsing, indexing, rendering, filesystem traversal, and Therion execution asynchronous, cancellable, debounced, or revision-keyed.
- Keep validation and problem reporting centralized in the Validation surface. Structure remains an orientation and navigation view, not a second validator.
- Move project validation toward a live diagnostic projection that updates from source/project changes. Manual validation actions are transitional controls, not the long-term primary workflow.

## Dependency Direction

The intended dependency direction is:

1. Core/domain (`src/core/**`)
2. Platform/infrastructure adapters (`src/platform/**`, filesystem/session/process/catalog adapters)
3. Application workflow services and controllers (`src/app/**` non-widget services/controllers)
4. Presentation shells and widgets (`src/app/**` widgets, dialogs, QGraphicsItems, scene/view code)

Rules:

- Core/domain code shall not depend on QtWidgets, QGraphicsView, dialogs, windows, or UI event classes.
- Widgets and scene items may orchestrate interactions, but shall not own parsing, serialization, persistence, command-catalog rules, process execution, platform-path decisions, or source rewrite logic.
- Production infrastructure shall be composed at explicit boundaries such as `main.cpp`, `MainWindow` bootstrap code, or a future composition root.
- Tests should pass fakes or explicit test adapters for IO, settings, sessions, catalogs, and process execution.
- C++ tests should use QTest for new coverage while CTest remains the repository-level orchestrator. QTest executables should be grouped by dependency and runtime boundary: core-only logic, app services, text-editor/widget support, map-editor/offscreen UI, and special integration/process tests. Prefer aggregate runners for many small same-boundary tests, but keep tests isolated when they are slow, flaky, UI-heavy, process-backed, resource-sensitive, or useful as crash-containment boundaries.
- Static/global access is acceptable only for deterministic, stateless transformations with no hidden IO, mutable cache, settings, resource, or platform dependency.

## Source Model Architecture

Therion Studio uses one shared, lossless source model for `.th`, `.th2`, and `thconfig` files.

The source model preserves:

- physical source lines
- comments and blank lines
- indentation where practical
- newline style
- encoding metadata
- source file type
- token spans and source offsets
- logical command ranges across continuations
- block ranges and parent stacks
- unknown but valid Therion directives

The main source-model contracts are:

- `TherionSourceText` for physical source text, line endings, offsets, and source-preserving text reconstruction.
- `TherionSourceDocument` for lossless physical-line snapshots, parsed source lines, source metadata, source roles, block
  ranges, and source lookup by line or offset.
- `TherionSourceLogicalDocument` for continuation-aware logical commands, command/token/option source ranges, block
  context, catalog metadata, and logical command lookup.
- `Th2GeometryProjection` for read-only `.th2` point, line, area, scrap, map, geometry, and background metadata
  projection used by Map consumers.
- `TextEditorSourceTransactionController` and map source transaction helpers for user-visible source mutations, undo/redo,
  dirty state, revision checks, projection invalidation, and selection/cursor restoration.

Consumers should use shared source snapshots and logical command metadata where available:

- Raw editor: highlighting, completion, context help, validation tooltips
- Blocks editor: cards, nesting, details, data rows, insertion/move rules
- Map editor: TH2 object projection, references, geometry source ranges, inspector edits
- Structure: project orientation, ownership hierarchy, map/scrap relationships, non-hierarchical project graph links
- Validation: active-document and project-level diagnostics
- Compiler workflows: target config resolution and source-aware feedback

Do not solve projection drift by copying parser logic into UI renderers, inspectors, sidebars, scene items, or completion code. Extend core parsing/token/range/catalog metadata instead.

Direct `TherionDocumentParser::parseLine`, `parseTokenLines`, and `tokenizeLine` calls are legacy-sensitive and are
guarded by `scripts/check_structure_constraints.py`. New production call sites should not be added unless they are a
documented core/synthetic exception such as parser implementation, logical source construction, validator safe-fix
snippets, command-option snippets, source-editor single-line rewrite helpers, or map pending-insert snippets that do not
have source text yet.

Map raster background layers hold a full-resolution (display-capped) pixmap and are sized into the preview canvas through an item-level transform, not by baking the source down to preview pixels. This keeps backgrounds sharp under view zoom and separates pixmap updates (source load, Gamma) from placement (position/scale), so repositioning and refit do not re-rasterize. `.xvi`/vector reference layers remain vector-painted projections and are unaffected.

Background metadata parsing and writing is a source projection boundary. XTherion `xth_me_image_insert` remains the default compatibility format for simple placement, while Mapiah `image_insert_v1` metadata is used for advanced background transforms such as rotation, non-uniform scale, and custom pivot. Treat both formats as import/export representations of one background placement model; do not duplicate placement state into parallel editor-specific metadata records.

Therion namespace and reference semantics are compatibility-critical. In particular, qualified survey references use Therion's innermost-to-outermost order (`child.parent`), not filesystem-style `parent.child`; project-index and validation code should follow `docs/THERION_COMPATIBILITY.md` before adding or changing namespace logic.

## UI Surface Ownership

The optional Map altitude underlay uses `ScrapAltitudeModel` for pure mean/relative
height and categorical fallback policy. `ProjectAltitudeService`, composed by
MainWindow, consumes existing asynchronous output inventory and loads bounded
survey/station-only LOX metadata off-thread; stale generations cannot publish.
Project-index station resolution exposes the canonical qualified reference rather
than duplicating namespace resolution in a renderer. `MapEditorAltitudeUnderlay`
projects cached source station mappings and paints mouse-transparent passage paths;
Map scene completion and partial-edit completion refresh this presentation only.
LOX has no reliable absolute-datum declaration here, so this feature reports
relative entrance heights only. Multiple LOX outputs deliberately use fallback.
XVI traverse coloring reuses that bounded LOX metadata and project-index resolution:
its station table supplies endpoint identities, and only uniquely resolved pairs
receive interpolated palette colors. The drawing's 2D coordinates never imply
height; unresolved pairs retain the configured centerline color. The sidebar legend
reports relative numeric heights and explicitly disclaims an absolute datum.
`ScrapAltitudeModel` normalizes LOX names to project-index lookup keys before
detecting collisions. Metadata carries output availability/ambiguity/load-failure
state; the legend reports actual colored/fallback leg counts and the reason,
without guessing an output or a datum.
Underlay contour reconstruction is separate from ordinary area clipping: spatially
indexed reciprocal nearest endpoints form independent closed components, using
the existing one-preview-unit connection tolerance. Explicit `outline in` paths
remove fill, `outline none` is excluded, and explicit non-wall outlines participate.
The presentation reports precise/approximate/unfilled scraps. Approximate fallback
uses spatially indexed shortest endpoint pairing only for scraps without a precise
outer contour, with local extent and inferred/known-length budgets. Coincident
branch junctions are rejected; dense endpoint buckets and candidate counts are
bounded. No bounding boxes or unrestricted gap bridges replace passage geometry.

The main project sidebar surfaces have distinct responsibilities. `Files`, `Survey`, and `Map` may be presented as tabs in one project navigation sidebar opened from the `Structure` rail entry, but their ownership stays separate:

- `Files`: filesystem/project file operations and file navigation.
- `Structure`: orientation and navigation for project objects and relationships. It may show structural relationship warnings that affect orientation, such as unresolved map/scrap composition references, but shall not become the general validation UI.
- `Validation`: all validation and problem identification, including command/catalog diagnostics, context/document-type diagnostics, parse/round-trip-sensitive findings, safe fixes, and project-wide problem scans.
- `Compiler`: Therion execution, target config selection, run output, and compiler-facing workflow state.

If a problem belongs in both navigation and validation, prefer showing it in Validation first. Add Structure surfacing only when it improves orientation without duplicating the validator workflow.

## Source Mutation and Transactions

Every user-visible source mutation should be one logical transaction with:

- a clear undo label
- revision/snapshot validation
- dirty-state update
- projection invalidation or refresh policy
- selection/cursor restoration policy
- focused undo/redo or round-trip coverage

TH2 map-editor source mutations shall route through the shared atomic map-source helper (`applySourceTextChangeWithSnapshot`) or an equivalent single-transaction abstraction that performs source replacement and undo snapshot creation together.

New Raw, Blocks, Map, inspector, sidebar, and project workflows that mutate source text should route through `TextEditorSourceTransactionController` or its successor shared source-change service. Temporary exceptions must be narrow, documented, and covered by focused tests.

## SQL Report Execution

SQL report import and query execution target a dedicated worker-thread boundary:

- `TherionSqlReportWorker` owns `TherionSqlReportDatabase` and creates it only after the worker has entered its affinity
  thread.
- The report subsystem owns a direct SQLite C connection created, queried, and closed on that same thread; widgets shall
  never receive or retain native SQLite handles or prepared statements.
- Import and query requests/results cross the boundary as immutable value DTOs carrying request and source identity,
  execution policy, schema/table data, and explicit error/cancellation state.
- A shared report execution control carries only atomic request/deadline state and a mutex-protected native connection
  pointer. The session may use that seam to call `sqlite3_interrupt()` from another thread, while connection detach and
  close remain serialized on the worker thread so interruption cannot race a stale handle.
- A SQLite progress handler enforces a monotonic per-query deadline and observes supersession or shutdown cancellation;
  cancellation and timeout remain distinct worker result states.
- Connection teardown occurs only after statement-local query objects have been destroyed.
- `MainWindow` explicitly composes a `TherionSqlReportWorkerSession` and injects its narrow session contract into the
  report tab. The session owns thread dispatch and asynchronous teardown; the tab owns only presentation state and
  accepts results whose monotonically increasing request ID and source generation are both current.
- `MainWindow` also composes and injects the report preset/settings adapter and CSV file exporter. The tab owns preset
  interaction and save-dialog presentation, but neither constructs `QSettings` nor serializes or writes CSV data.

The async tab workflow suppresses superseded results, interrupts active SQLite execution when a newer request arrives,
and keeps closing bounded by disconnecting UI publication before requesting worker shutdown. Read-only queries use a
ten-second execution deadline; timeout, cancellation, recovery, and interrupted teardown are covered with recursive-query
worker tests.
The report database intentionally does not use QSQLITE: Qt's driver reports query cancellation as unsupported and its
native handle cannot be paired portably with an independently linked SQLite library. The isolated report subsystem
instead links the platform-owned SQLite API directly (`SQLite3::SQLite3` on macOS/Linux and Windows SDK `winsqlite3`),
which gives the worker one stable handle for interruption and progress handling without Qt private APIs.

## Validation Architecture

Validation should be conservative and catalog-backed.

- Active-document and project validation should use the same source metadata and catalog rules where practical.
- `TherionSourceValidator` and project validation scanners are the only owners of warning/error diagnostics. The raw syntax highlighter may color normal token categories and may render supplied diagnostic ranges, but it must not invent independent warnings/errors.
- Any diagnostic underline/background in Raw mode should be traceable to a validator diagnostic surfaced through the Validation panel or validation tooltip.
- Project validation should become a live, background diagnostic projection over the open project graph and open in-memory documents. It should react to source edits, saves, file additions/removals, project-open events, and catalog/source-model updates through debounce, cancellation, and revision or generation IDs.
- Project validation should detect deterministic source/project consistency problems such as missing `input`/`source` files, catalog command/option/context/document-type issues, malformed option tokens, unclosed blocks, duplicate IDs in Therion namespaces, unresolved map/scrap/join/station references, and area references to missing line IDs.
- A manual project validation button may remain during migration or as an explicit refresh/re-run action, but the target workflow is automatic problem discovery in the Validation panel when project problems appear or disappear.
- Unknown commands, unknown options, document-type diagnostics, and context diagnostics should remain warnings unless the rule is fully deterministic.
- Safe fixes must be explicit source edits with known ranges and must not silently skip undo snapshots or dirty-state handling.
- `thconfig`, `.th`, and `.th2` document-type applicability should come from command metadata and source-type detection, not UI heuristics.
- False-positive prevention should be backed by focused tests using real Therion patterns where possible.
- Project validation diagnostics are owned by the Validation panel and raw-editor diagnostic projection. They shall not drive map-editor scene refresh, selection restoration, hit testing, drag state, source snapshots, or embedded map-editor highlighters unless a future behavior is explicitly scoped and covered by map interaction regression tests.

## Project Index and Structure

`ProjectStructureIndex` owns lightweight project orientation data:

- root config/source inference
- survey, centerline, map, scrap, station, point, line, and area entries
- stable object IDs
- source file and line navigation
- map/scrap composition references
- non-hierarchical relationships such as preview, revise, join, and equate when represented as graph data

The project navigation sidebar displays file navigation and project structure projections. Its default `Survey` tree follows survey namespace and definition ownership; map composition references are graph relationships and must not reparent map or scrap definition nodes in that projection. A separate `Map` projection may display map composition using those graph relationships. Structure should not list general validation diagnostics. Validation-owned diagnostics should be surfaced in the Validation panel and may reference project-index objects when that improves navigation.

Project source and project-index cache ownership belongs at an explicit application workflow boundary. Structure and Validation scanners may share a `ProjectScanCacheService` composed by `MainWindow` or a future composition root, but they shall not use static/global project-index caches or hidden service locators.

Validation prunes source projections to the current snapshot and catalog at the scan boundary, before borrowing
projection references. Unchanged projections remain reusable; shared logical-document handles retain their own lifetime.
Path ordering precomputes canonical and case-folded keys before sorting rather than performing filesystem lookups
inside comparators. In-memory aliases preserve the original-path tie-break for deterministic override precedence.

Project watch inventory discovery belongs to `ProjectFileWatchInventoryCollector`, a pure application-level filesystem
component. It normalizes the root, applies shared project discovery skip rules, and returns deterministic paths,
signatures, skipped paths, and errors; it has no presentation or `QFileSystemWatcher` dependency. A generation-keyed
workflow service shall run that collector off the GUI thread, while `QFileSystemWatcher` path mutation and completed
inventory application remain GUI-thread responsibilities.

The Therion compiler remains authoritative for final export behavior. The project index is a lightweight navigation and early-feedback projection, not a compiler replacement.

Project-index namespace, reference, and duplicate-identifier behavior shall follow `docs/THERION_COMPATIBILITY.md`. In particular, survey/map/scrap names share their parent survey namespace, drawing object IDs share their containing scrap namespace, and identical object names in different survey namespaces are valid.

## Catalog and Metadata

Therion command, option, help, style, and symbol metadata should be data-driven.

- Prefer parser/generator fixes over editing generated catalog output by hand.
- Use `resources/therion_catalog/overrides/*.override.json` only as a short-term fallback when parser extraction is not yet feasible.
- Remove overrides once generator support exists.
- Keep command catalog loading at composition, startup, or explicit test setup boundaries. Avoid UI-side static catalog access and long-lived resource overrides.
- Map-object style discovery and override merging shall occur at Map-editor composition or explicit test setup. The
  resulting immutable catalog is passed to the scene renderer and style preview; rendering and painting shall not load
  resources, inspect environment/settings paths, or access a process-global catalog cache.
- Map line-decoration items project the unchanged path to logical device coordinates for nominal glyph metrics and visible-interval repetition. Explicit slope `l-size` values remain source-derived geometry. The scene lifecycle synchronizes view-dependent paint bounds and direction-aid orientation without source parsing or scene replacement; fit bounds exclude nominal screen-space margins. Drag previews refresh slope vertex metrics alongside their path.
- Background asset caching is an application-owned, byte-bounded LRU policy. Cache keys shall include stable normalized
  absolute source identity, asset format, and decode/projection options; cached values retain revision/error state but no graphics-item,
  inspector, or source-transaction state.
- `MapEditorBackgroundAssetLoader` owns cached background-source reads and the deterministic XVI/SVG decoding needed
  to produce immutable asset payloads. `MapEditorBackgroundLayers` owns placement, graphics-item application, and
  background UI lifecycle; neither responsibility owns the other’s state.
- Raster gamma previews keep at most one active worker and the latest pending value per layer. Every selection,
  including a cached preview, supersedes older results; layer/source invalidation drops pending values.
- A Map source revision is handed to scene refresh through `MapEditorSourceProjectionSnapshot`, held as a
  `shared_ptr<const ...>`. It owns the logical commands, TH2 geometry projection, and source bounds for that one
  revision, so scene refresh shares one immutable identity rather than copying separately cached projections.
- `MapEditorSceneGeneration` advances when an accepted full or single-feature partial Map refresh begins. Deferred
  selection recovery captures that generation and shall not mutate the scene after a newer refresh supersedes it.
- `sceneProjectionRefreshCompleted(generation)` is emitted only for the current generation after the relevant
  selection/navigation restoration. `sourceDrivenMapRefreshCompleted` remains a separate legacy source-refresh seam.
- Project watcher mutations invalidate matching background assets in every attached or detached Map tab. The source
  identity is a normalized absolute path so invalidation remains stable after a file is removed; raster reload stays
  asynchronous, while vector items replace only their immutable source payload or derived geometry.

## UI Shell Reduction

The compiler console retains at most 10,000 document blocks for rendering. An explicitly composed
`TherionConsoleOutputStore` owns the complete UTF-8 transcript on disk, independently of the visible preview.
Copy reads the complete transcript only on explicit user request; log failures are surfaced and prevent partial-copy success.

`MainWindow`, `TextEditorTab`, `MapEditorTab`, and `ThreeDViewerTab` are orchestration shells under active reduction.

When touching these classes:

- prefer extracting focused non-widget collaborators over adding private state and branches
- keep callbacks narrow and ownership-aware
- prefer named QObject signals/slots or typed context interfaces for semantic workflows
- avoid broad service-locator context structs
- keep QGraphicsItems presentation-focused: rendering, hit affordances, and local interaction state only

## Directory Contracts

The editor-mode layout is stable:

- `src/app/text_editor/` for raw editor shell and shared text-editor orchestration
- `src/app/text_editor/raw_editor/` for Raw-specific components (`RawEditor*`)
- `src/app/text_editor/block_editor/` for Blocks-specific components (`BlockEditor*`)
- `src/app/text_editor/map_editor/` for Map/Visual-specific components (`MapEditor*`)
- `src/app/three_d_viewer/` for the read-only 3D viewer shell and viewport host
- `src/core/ThreeDViewerSceneModel.*` and `src/core/ThreeDViewerCamera.*` for the shared 3D viewer scene contract and camera state, including orbit, pan, zoom, fit, reset, preset views, and world-blue-axis rotation
- `src/app/three_d_viewer/ThreeDViewerLayerListModel.*` for shared layer visibility and counts data that can drive both QWidget and QML surfaces
- `src/app/three_d_viewer/ThreeDViewerInspectorState.*` for QML-ready scene summary data and mesh-coloring mode exposed to the 3D viewer inspector
- `src/app/three_d_viewer/ThreeDViewerInspectorWidget.*` for the QQuickWidget host that embeds the inspector shell into the Widgets application
- `src/app/three_d_viewer/ThreeDViewerViewportItem.*` for the QQuickItem-based GPU-backed viewport surface that renders the shared scene model through the Qt Quick scene graph
- Map-to-3D opening passes the current source snapshot and Objects-panel hidden source lines explicitly. `TherionScrap3D` filters hidden scraps and objects before projecting line and point geometry; station anchors remain local to their owning scrap, even when anchor symbols are hidden. Opening a TH2 scopes geometry to that file, while its survey source still supplies the solved centreline.
- 3D wall segments share vertex-colored scene-graph batches per stroke width; changing the camera does not allocate a material and node for every altitude-colored segment.
- Map-to-3D uses the `objectsOnly` projection path: visible lines except borders and invisible walls, and non-station point objects, are draped without building passage or area meshes. Non-wall strokes are capped at 45% of the base wall width; point outline paths reuse the TH2 symbol geometry helpers and join the same width/color batches. Passage fill generation and rendering are disabled; Map underlays are unchanged.
- `src/app/three_d_viewer/ThreeDViewerViewportItem.*` also owns the mesh-group scene-graph material choice, currently using Qt's built-in GPU vertex-color material for shaded triangle output
- `src/app/three_d_viewer/ThreeDViewerViewportItem.*` also owns screen-space HUD overlays such as the scene bounding box, compass, scale bar, altitude legend, hover station details, and measurement readouts
- `src/app/three_d_viewer/ThreeDViewerViewportWidget.*` for the QQuickWidget host that embeds the viewport item into the Widgets application
- `src/app/three_d_viewer/ThreeDViewerProjection.*` for viewer-space projection math shared by the widget host and future render surfaces
- `src/app/three_d_viewer/ThreeDViewerViewportController.*` for widget/QML-ready camera interaction, viewport command handling, and camera-change signaling

Do not add new editor-mode implementation files back into legacy top-level `src/app/` paths when they belong to this layout.

Any intentional layout migration shall update:

- `CMakeLists.txt`
- structure guardrails/tests
- `AGENTS.md`
- `ARCHITECTURE.md`
- `WORKLOG.md`
- relevant specification/manual sections when behavior changes

## Verification Expectations

Architecture-sensitive changes should include focused verification:

- parser and source model changes: round-trip, source-range, continuation, comment/blank-line, and encoding-sensitive tests
- validation/catalog changes: false-positive and document-type/context tests
- source mutation changes: undo/redo, dirty-state, revision mismatch, and projection refresh tests
- project index changes: root config/source inference, in-memory document state, namespace/reference resolution, and stable object IDs
- Therion compatibility changes: nested namespace order, relative references, duplicate names/IDs in the same namespace, and same-name objects in different namespaces
- UI workflow changes: relevant widget/controller tests or explicit manual verification when automation is not feasible

Before committing or proposing a PR, run `python3 scripts/check_structure_constraints.py` and fix violations in the same change.
