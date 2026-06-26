# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. Release readiness for `v2026.6.8`.
2. Unified Source DOM and source transaction ownership.
3. Test infrastructure hygiene and structure guardrails.
4. 3D viewer GPU-backed viewport rollout and shell migration.

## Active Work

### Release Readiness

- Run local validation before tagging or packaging handoff, including focused map inspector regressions touched during release stabilization.
- Keep Selection inspector point-geometry ordering, `-clip off`, and narrow-width layout regressions covered while stabilizing `v2026.6.8`.
- Keep status-bar mode/compiler badges readable without visually competing with primary actions.
- Keep Selection inspector terminology consistent: use `Options` for object-level settings and reserve `Line Point` for vertex workflows.
- Keep Selection inspector object-level actions grouped under `Options`, including `Name`/`Text`/`Value`, scrap projection, and the full object-settings entrypoint.
- Keep Selection inspector `Options` ordering deterministic across refreshes; visible rows must not jump when selection state or option visibility changes.
- Keep map inspector numeric spin boxes from committing partially typed values before editing is finished.
- Keep map path hit-testing from letting hidden line control handles steal clicks from visible nearby paths.
- Keep low-zoom map path hit-testing deterministic: the highlighted object under the cursor must be the object selected
  by click, selected/hovered stale items must not steal clicks, and path candidates outside the current screen-space hit
  radius must not become inspector primary selections.
- Keep hidden gated map vertices reachable for context-menu selection paths without making ordinary hidden handles steal primary clicks.
- Keep visible map vertex/control-handle affordances ahead of thick selected paths in viewport hit-testing so clicks near
  the drawn handle fall through to QGraphicsItem drag handling instead of being consumed as path reselection.
- Keep direct map vertex/control-handle presses carrying pending line/vertex metadata so selection refreshes restore the
  clicked line point instead of drifting to another selected item on the same or nearby geometry.
- Keep Windows pen-tablet and stylus input normalized through the map viewport controller so selecting, line/area
  drafting, and freehand strokes do not depend on platform mouse-event synthesis or duplicate generated clicks.
- Keep map panning discoverable through temporary `Space` + primary-button drag and `Ctrl` + primary-button drag while
  preserving right-button drag, precision-scroll panning, context menus, and the currently selected drawing tool.
- Keep map vertex and Bezier control point markers large enough to target comfortably on HiDPI/Windows-scaled displays
  without changing source coordinates or map geometry.
- Keep Bezier control point markers visually distinct as solid blue square handles.
- Keep Bezier control connector lines visible enough on light raster backgrounds and HiDPI displays.
- Keep standalone point/station anchors out of snap guide markers during line/area vertex drags; show them only as the
  active snap target when the dragged or inserted vertex is actually within snap range.
- Keep snap guide and active target markers as scene-anchored overlay items so they do not drift with the dragged vertex.
- Keep map tabs loaded while hidden, such as New Project from Template tabs, refreshing once when first shown so handles
  and drag affordances are attached to the visible viewport state.
- Keep inactive `.th` / `.thconfig` tabs from driving map-object cursor synchronization; only the current document should
  update the global map object selection from its current line.
- Keep Raw editor `input` path completion replacing the active path token, including `./`, mid-token, quoted, and
  Windows-style separator cases, so accepting a suggestion cannot duplicate the existing suffix.
- Keep the Raw editor find/replace bar closable with `Esc` from either the find/replace controls or the text editor while
  the bar is open.
- Keep map line selection readable by focusing clicked segments: primary path clicks should expose only the segment
  endpoint anchors and their control handles, with smaller visible vertex markers.
- Keep map line-point orientation/size handles draggable when they overlap highlighted paths, and keep Backspace/Delete
  deleting a whole selected line unless an actual vertex/control handle is selected.
- Keep `line slope` orientation/size arrow handles targetable directly at the arrow head without extending their hit area
  back across the connector segment or stealing endpoint vertex drags.
- Keep draft completion and draft auto-commit from recentering the map viewport or force-scrolling the Objects tree to
  the newly inserted object.
- Keep pending point/line/area recent type/subtype MRU choices persisted across application restarts.
- Keep repeated `point station` draft naming compatible with numeric and trailing-letter survey conventions such as
  `1a -> 1b` and `1z -> 1aa`.
- Keep map geometry strokes readable on HiDPI/Retina displays without changing TH2 source geometry or XTherion
  background-reference loading semantics.
- Keep map object style catalog tests aligned with intentional resource-backed visual tuning such as `line wall`
  stroke width changes.
- Keep interactive map drawing previews and control guides readable on HiDPI/Retina displays while preserving their
  transient UI-only behavior.
- Keep interactive line/area drafts editable before commit: captured anchors and their Bezier controls must both be
  movable without writing source text until the draft is completed.
- Keep map draft geometry insertion from writing into TH2 files with unclosed scraps; the map editor should surface the
  missing `endscrap` instead of appending fallback geometry into an ambiguous source structure.
- Keep unclosed `.th2` `scrap`, `line`, and `area` validation findings fixable through explicit `Apply Fix` insertions of
  `endscrap`, `endline`, or `endarea` at conservative map-command boundaries.
- Keep line extension completion as one source transaction so `Extend Before/After` exits draft mode only after the
  extended line rewrite, selection restore, and UI refresh hook run together.
- Keep `Extend Before/After` covered by a transaction regression so successful commits exit draft mode and preserve undo/redo.
- Keep map object-details and object-tree source transactions result-aware so stale or skipped edits do not overwrite
  transaction status with success messages.
- Keep map background metadata writes result-aware so skipped XTherion/Mapiah metadata syncs report the transaction
  result directly without depending on post-apply hooks.
- Track SVG background support through `plans/SVG_BACKGROUND_PLAN.md`; SVG backgrounds can now be added from the
  Backgrounds file picker and round-trip through Mapiah `format=svg` intrinsic size/source viewBox metadata, with a
  workflow regression using a real Wikimedia cave-map SVG fixture for insertion, transform preservation, and removal.
- Keep raster/SVG background pivots stored in layer-local image coordinates so `Set Pivot` works consistently for clicks
  inside or outside the visible background and subsequent scale/rotation keeps the chosen scene point fixed.
- Keep manually imported XVI backgrounds visible in the Backgrounds inspector immediately after import, and keep XVI
  position edits writing Mapiah base-position metadata instead of only moving the transient scene item.
- Keep tab-level map source insertions such as Insert Scrap, Complete Draft, and Smart Area result-aware instead of
  inferring success from post-apply hooks.
- Keep map background layer refreshes isolated from viewport-only command-surface updates so loaded raster/XVI metadata
  does not cause inspector blinking or interrupt line vertex/control-handle interaction.
- Keep raster background position edits using the same transform-metadata path as scale/rotation so inspector spin boxes
  refresh immediately and X/Y stepping stays monotonic.
- Keep legacy XTherion raster background placement anchored by metadata top-left semantics even for mixed-size multi-layer
  scans and stale restored sessions, while preserving Mapiah scale/rotation pivot behavior.
- Keep map selection restoration scoped to the narrow line-vertex restore generation used before `fb965aa`; point,
  anchor, inspector, and line-extension restores must not share a broad map-selection veto counter.
- Keep MainWindow project/sidebar refresh work out of synchronous map-editor source-change handling so project-mode
  structure/object updates cannot re-enter canvas selection or drag release processing.
- Keep new raster background insertion metadata compatible with XTherion defaults without changing how existing
  XTherion-authored raster background references are loaded.
- Keep Objects inspector scrap expansion user-driven by default: source refreshes and text cursor navigation preserve manually
  collapsed scraps, while selecting a map object inside a collapsed scrap reveals that object. The persisted
  `Auto-collapse/expand Scraps` toggle switches the inspector to a focused mode that keeps only the selected
  object's scrap expanded without briefly expanding or repainting other scraps during source-driven object-tree
  refreshes, including raw-editor navigation while the map pane is detached.
- Keep pending map drawing defaults local to the open map editor: Point, Line, and Area remember deduplicated recent
  type/subtype choices, expose them as compact wrapping quick buttons below symbol preview, and use the newest choice
  for the next new object instead of resetting to built-in defaults every time.
- Keep repeated pending `point station` drawing ergonomic by advancing the next `Name (-name)` from the last committed
  station point name while preserving survey suffixes, keeping the panel on the next point draft, and clearing the
  pending station name when the user changes the next point type away from `station`.
- Keep pending line/area drawing panels available during drafting: show `Options`, expose forward-applied `Line Point`
  control handles, smooth, and subtype controls before each click, expose `orientation` / `l-size` only for `line slope`,
  write supported standalone line-point rows into the committed draft, and clear line-point subtype for every new draft.
- Keep newly committed pending point/line/area tools focused on the next draft instead of switching the Selection panel
  to the just-inserted object, so recent choices and draft fields remain available for repeated drawing.
- Keep `Smooth (-smooth)` behavior visual and source-backed: disabling it removes line-point control handles immediately
  for draft and selected lines, and committed line tools return to the next line draft after insertion.
- Keep single-key map drawing shortcuts focused on canvas workflows: `P`/`L`/`A` start object drawing,
  `R`/`C` toggle selected line state or close the active line draft, and `S`/`,`/`.` adjust selected or next line-point
  smooth/control-handle state without firing from text-editing widgets.
- Keep selected map line/area vertex markers close to XTherion styling: larger red anchor circles with blue
  outlines, smaller blue control-point squares, and a red focused-vertex halo.
- Keep map selection restoration generation-keyed so stale delayed restores cannot reselect old or nearby point/line
  scene items after source-driven scene refreshes.
- Keep block-editor canvas selection refresh coalesced so transient empty selection states do not flash the Selection
  inspector or interrupt block item dragging.
- Render empty full-line comment commands in the block canvas and keep block-card reordering independent of
  platform-specific `QGraphicsItem` implicit move behavior.
- Keep empty scrap object cleanup explicit through validator warnings and `Apply Fix`, not silent source mutation.
- Keep deletion-style validation fixes visibly labeled as removals and preview the full source block being removed.
- Keep validator `Apply Fix` source edits refreshing open map projections immediately after a successful transaction.
- Keep validator `Apply Fix` navigation synchronized with the next selected problem after the refreshed findings list is
  rebuilt.
- Keep inline `type:subtype` map object rendering aligned with inspector preview and `-subtype` rendering.
- Keep Selection quick-field label/input visibility synchronized with wrapper visibility across clear/refresh cycles.
- Keep point `-align` rendering anchored like Therion so map canvas placement matches saved point options.
- Clear pending Selection inspector insert state when Smart Area confirmation returns to Select mode.
- Keep release notes, README, package metadata, and CI artifact workflow aligned with `v2026.6.8`.
- Keep Therion executable auto-detection fallback candidates build-clean after the detector PR so release packaging
  and desktop startup validation do not fail on a trivial compile regression.
- Keep Linux CI and package-builder Qt dependency lists aligned with the Qt Quick/QML-backed 3D viewer surface.
- Keep Windows CI and installer Qt archive lists aligned with the Qt Quick/QML-backed 3D viewer surface.
- Keep Windows installer deployment passing `--qmldir resources/qml` to `windeployqt` and smoke-checking deployed
  `bin/qml/QtQuick/Controls`, `Layouts`, and `Templates` so the 3D viewer inspector does not install as a blank Quick surface.
- Keep cross-platform diagnostic logging available behind `THERION_STUDIO_ENABLE_LOG=1`, with `THERION_STUDIO_LOG_FILE`
  enabling a path override while investigating Qt/QML runtime failures that do not surface in a console.
- Keep AppImage runtime-library staging aligned with Debian Qt runtime dependencies instead of masking missing
  bundled libraries in smoke-test containers.
- Keep AppImage package/smoke scripts diagnostic enough to identify whether missing runtime libraries were lost
  during AppDir staging or final AppImage packaging.
- Keep dynamically loaded AppImage runtime backends, such as libproxy's `libpxbackend-*.so*`, staged explicitly
  because they are not guaranteed to appear in direct `ldd` dependency scans.
- Keep AppImage runtime backend discovery recursive within system library roots so distro-specific subdirectories
  such as libproxy module directories are bundled before smoke testing.
- Keep dynamically loaded AppImage backend modules copied directly into `AppDir/usr/lib` after discovery so final
  package validation can prove they are present even when they are not ordinary `ldd` dependencies.
- Keep AppImage dynamic backend dependency closures bundled for arm64 smoke tests, including libproxy's curl,
  GSSAPI, Kerberos, LDAP, and TLS support libraries.
- Keep AppImage runtime backend diagnostics explicit enough to show searched roots and matched candidates when
  distro runtime modules are missing from the AppDir.

### Unified Source DOM / Transactions

- Tighten source-file reference resolution while preserving Therion namespace semantics from `docs/THERION_COMPATIBILITY.md`.
- Keep source transaction helpers result-aware so callers can distinguish applied, stale, no-op, unavailable, and invalid
  source edits without relying on selection-restore hooks as success signals.
- Continue propagating transaction results through map-editor inspector/object-details contexts before migrating broader
  source-write ownership.
- Continue propagating transaction results through map-editor background metadata workflows without changing XTherion
  background-reference loading semantics.
- Keep map-editor `applySourceTextChangeWithSnapshot` callers audited so user-visible insertions only report success
  after an applied transaction result.
- Keep non-map text-editor source rewrites result-aware through `TextEditorSourceRewriteController` so system
  normalization, validation fixes, and cursor insertions can distinguish applied, no-op, stale, invalid, and unavailable edits.
- Keep `TherionSourceSnapshotCache` invalidation covered for revision, undo/redo-style revision replay, encoding,
  source type, catalog revision, and uncached catalog projections before migrating more Structure/Validation consumers.
- Slice 2 - Continue expanding shared source projections into remaining Blocks/Map consumers now that app-side validation,
  project-index, raw completion/context, validator string overloads, transient cache paths, and the Blocks canvas
  rebuild/document-outline loops and block-boundary matching share `TherionSourceSnapshotCache` projections.
- Slice 4 - Migrate Blocks cards/details/move planning toward shared logical command and option ranges while preserving one source transaction per user-visible change.
- Slice 5 - Migrate Map/TH2 object discovery, generic option parsing, reference resolution, and Smart Area insert planning to shared logical commands while keeping map geometry parsing map-specific; continue switching remaining Map inspector reads after quick fields, `-clip`, `-align`, and structured line-point metadata reads to logical-command or parsed-feature ranges.
- Slice 6 - Close source transaction ownership by routing remaining user-visible source mutations through `TextEditorSourceTransactionController` or a narrow successor with undo label, expected revision, invalidation, dirty-state, and selection/cursor policy.
- Slice 7 - Reuse cached logical documents for Structure, project index, namespace/reference resolution, search, and live Validation diagnostics.
- Slice 8 - Remove duplicate editor-local tokenizers, option parsers, line splitters, numeric classifiers, and source-range heuristics only after migrated consumers have regression coverage.

### Validation And Catalog Metadata

- Keep validation conservative while moving command, option, and positional argument interpretation into catalog-backed logical-command metadata.
- Keep problem reporting centralized in the Validation panel while Structure remains an orientation/navigation view.

### Test And Structure Hygiene

- Use QTest for new C++ tests while keeping CTest as the runner.
- Keep `tests/core/` and `TherionCoreQTests` as the baseline pattern for small core-only QTest cases.
- Keep `MainWindowServiceQTests` as the baseline pattern for small MainWindow project/session service tests that share `therion_app` dependencies.
- Keep `TextEditorDocumentServiceQTests` as the baseline pattern for small text-editor document IO/state/precondition/workflow service tests.
- Keep `TherionRunnerSupportQTests` as the baseline pattern for Therion runner config, executable-selection, and presenter support tests; keep real process runner tests isolated.
- Continue migrating touched hand-rolled tests to QTest where the dependency/runtime boundary is already clear.
- Keep `python3 scripts/check_structure_constraints.py` green and preserve guardrails against map-editor source mutation bypasses.
- Keep the explicit user-confirmation gate before every `git commit`.
- Keep optional sample-data dependent tests from aborting CI when fixture directories are absent.
- Keep small committed map-editor regression fixture projects, including XVI-background and real cave raster/LOX
  variants, for selection, vertex/control-handle drag, and project-open smoke coverage without depending on local
  sample data.
- Keep shared workspace command bars palette-aware across light and dark appearance changes.
- Keep Linux CI/package Qt runtime module lists aligned with QML inspector imports.
- Keep UI smoke tests deterministic across platform event-loop timing differences.

### UI Cleanup

- Continue `plans/GUI_CLEANUP.md` in independently shippable slices.
- Keep style policy, UI construction, presentation contracts, and source/model logic separated.
- Keep `plans/3D_VIEWER_PLAN.md` as the planning reference for remaining 3D viewer refinement work.

### 3D Viewer

- The `.lox` loader and neutral scene model are implemented in `src/core/` and covered by `TherionCoreQTests`.
- The `ThreeDViewerCamera` model now lives in `src/core/` and owns orbit, pan, zoom, fit, and reset state for the viewer shell.
- The viewer projection helper now lives in `src/app/three_d_viewer/` and is covered by a dedicated `ThreeDViewerQTests` runner.
- The 3D viewer toolbar now exposes top-view, side-view, and rotate-left/rotate-right controls alongside fit/reset controls, with rotation around the world blue axis.
- The `ThreeDViewerTab` host is integrated into the main window as a read-only `.lox` viewer tab with basic layer toggles, scene summary, and a first interactive viewport slice.
- The 3D viewer inspector now uses a Qt Quick/QML host embedded through `QQuickWidget`, with the inspector content rendered from a shared QML surface, grouped into scene/layer sections while the viewport is moving to a GPU-backed scene-graph host.
- The viewport rendering now lives in a QQuickItem-backed scene-graph surface so the viewer can render through Qt Quick instead of QWidget painting.
- Mesh groups now render through Qt Quick's built-in GPU vertex-color material on the scene graph path.
- The 3D viewer inspector now exposes a model-coloring mode that switches the GPU model palette between altitude coloring and no coloring.
- The 3D viewer centerline now uses the same altitude or uncolored palette as the meshes.
- The 3D viewer viewport now draws a red bounding box around the current scene extent.
- The 3D viewer viewport now overlays a compass, scale bar, and altitude legend when scene bounds are available.
- The 3D viewer canvas now uses a black background to match the Loch-style presentation.
- The 3D viewer centerline, stations, and labels now use higher-contrast rendering on the black viewport background.
- The 3D viewer station markers are now smaller and less visually noisy on dense views.
- The 3D viewer station markers and fully qualified station labels now use automatic screen-space decluttering instead of drawing every overlapping station annotation.
- The 3D viewer viewport now shows hover details for station markers, including full station reference, and supports a ruler-toggle measurement mode for station-to-station distance, azimuth, and vertical difference.
- The 3D viewer hover card layout now uses a more even padding balance and larger typography for station details.
- The 3D viewer viewport now overlays Underground Passages Length and Underground Depth, computed from underground centerline shots only and excluding surface, splay, duplicate, and surface geometry contributions. The altitude legend is shown only in depth coloring mode, the compass and view-angle indicator are grouped beneath it like Loch, the view-angle semicircle uses a horizontal split and signed upper/lower motion, the scale bar is a simple line with end ticks, and the altitude legend includes more intermediate labels.
- The 3D viewer scene statistics overlay now uses larger typography for the project title and underground passages/depth summary.
- The 3D viewer HUD scale bar is aligned to the compass row with a matching gap to the view-angle indicator.
- The 3D viewer toolbar now uses arrow-based icons for `Top View` and `Side View`.
- The 3D viewer core now exposes station qualified-name construction in the shared scene model and has broader `.lox` fixture-matrix coverage for survey hierarchy, shot flags, and synthetic terrain surface chunks.
- The transitional QWidget viewport renderer has been removed now that the scene-graph viewport is the sole active render path.
- The viewport controller is now split out from the widget shell so camera interaction and camera-change signaling can be reused by future render surfaces.
- The layer inspector is now backed by a shared list model so the QWidget view and future QML UI can use the same visibility/count state.
- The 3D viewer layer inspector now shows clean layer names without visible item counts, generates centerline sublayers from present shot classes, defaults centerline sublayers to underground-only visibility, exposes disjoint station sublayers only when multiple station classes are present, and keeps station markers and labels constrained to stations attached to currently visible centerline shots.
- The 3D viewer station hover and measurement picking now ignore stations whose centerline shots are hidden by the current centerline layer filters.
- The 3D viewer labels layer now remains independent from station marker visibility while still respecting the visible centerline filters.
- Viewer fit/reset controls now live in the shared workspace command bar instead of a tab-local toolbar.
- The 3D viewer layer list now blocks internal item-change recursion during tab construction and refresh.
- The 3D viewer toolbar now has a play/stop automatic-rotation toggle, and the inspector exposes rotation speed in degrees per second.
- The 3D viewer toolbar now orders view controls as reset, fit, orthogonal projection, top/side views, rotate controls, auto-rotation, and measurement, using a home icon for reset and standard rotate icons for manual yaw.
- The 3D viewer inspector now exposes precise camera sliders for compass heading, tilt, distance, and focal length while keeping those values synchronized with viewport navigation; focal length is disabled in orthographic projection.
- The 3D viewer Fit command now computes camera distance from the projected scene bounds using the current viewport aspect ratio, reducing excessive empty space around fitted `.lox` models.
- The 3D viewer removed survey-based model coloring; Altitude is now the default model-coloring mode, and None renders meshes in one solid light-gray color.
- The 3D viewer inspector Scene Settings section now appears after Layers and exposes default-on visibility toggles for the bounding box, full HUD overlay, and title/statistics overlay.
- The 3D viewer viewport no longer draws standalone world X/Y/Z axis guide lines over the scene.
- The 3D viewer Layers default now starts with Stations and Labels hidden, while underground centerline, meshes, and surfaces remain visible.
- The 3D viewer inspector now uses consistent Title Case for English field and layer labels.
- The 3D viewer now supports arrow-key yaw/tilt navigation, higher-contrast per-vertex mesh lighting, and a palette-aware QML inspector surface styled closer to the existing QWidget inspectors.
- The 3D viewer measurement mode now exits on `Esc`, keeping the viewport, inspector, and tab state synchronized.
- The 3D viewer arrow-key navigation and rotate buttons now use a shared 5-degree yaw/tilt step; toolbar view
  commands return focus to the viewport, and top/side view rotation keeps a stable compass heading while top-view tilt
  remains 90 degrees. Top/side view presets preserve the current compass heading instead of resetting yaw, and arrow-key
  navigation works whenever the 3D viewer tab is active.
- The project File sidebar now uses the same Therion badged document icon for `.lox` files as for `.th`, `.th2`, and `thconfig` files.
- Open `.lox` viewer tabs now auto-reload regenerated files through file and parent-directory watches while preserving the current camera and inspector state instead of refitting the view.
- Continue the renderer refinement and the broader Qt Quick/QML shell migration once the GPU-backed viewport proves out the document-open workflow.

## Blocked / Needs Input

- Old Therion/Metapost crash fixture: keep parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Replace remaining fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Retire or demote the manual `Validate Project` action after live project diagnostics are reliable for edits, saves, file operations, project-open, and catalog/source-model refresh events.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Bounded `.xvi` cache policy for very large projects.
- Station marker/label priority ranking follow-up: tune automatic decluttering if dense projects hide important stations.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX follow-up for hardware-specific pressure, hover, and tablet-driver behavior.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, stable raster position anchors during scale/rotation, XTherion rewrite caveats, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
