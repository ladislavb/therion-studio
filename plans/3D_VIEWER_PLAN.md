# 3D Viewer Remaining Work Plan

This plan tracks the remaining work for the read-only 3D `.lox` viewer in Therion Studio. The initial integration is no longer speculative: the loader, neutral scene model, Qt Quick scene-graph viewport, toolbar controls, inspector, overlays, measurement mode, and basic tests exist. Future work should refine the current implementation rather than restart the earlier discovery phases.

The old Therion Loch sources in `therion/loch` remain useful as a behavioral reference for rendering conventions, overlays, and format semantics, but Therion Studio should continue using neutral `three_d_viewer` naming and should not import the Loch GUI architecture wholesale.

## Current Implementation State

- `.lox` files open in a read-only 3D viewer tab.
- `.lox` loading lives in `ThreeDViewerLoxLoader` under `src/core/`.
- The neutral scene model lives in `ThreeDViewerSceneModel` and currently covers surveys, stations, shots, mesh groups, terrain surfaces, station flags, LRUD data, shot flags, and scene bounds.
- Camera orbit, pan, zoom, fit, reset, orthogonal projection, top view, side view, manual blue-axis rotation, automatic Z-axis rotation, precise compass-heading/tilt controls, distance control, and focal-length control are owned by `ThreeDViewerCamera` / `ThreeDViewerViewportController` / viewport state.
- The viewport is a Qt Quick `QQuickItem` scene-graph surface hosted in the existing Qt Widgets shell.
- The viewer renders centerline, stations, labels, mesh groups, surfaces, a red scene bounding box, and Loch-style screen overlays.
- The model coloring mode supports survey and depth palettes and applies consistently to meshes and centerline.
- The measurement workflow is toolbar-driven with station hover highlighting and station-to-station distance, azimuth, and vertical difference output.
- The viewport overlays cave length and cave depth computed from underground centerline shots while excluding surface, splay, duplicate, and surface geometry contributions, and shows the altitude legend only when model coloring is set to depth.
- The inspector is intentionally narrow: model-coloring control, camera/auto-rotation controls, and layer visibility toggles with clean layer names, no visible item counts, data-driven centerline sublayers defaulting to underground-only visibility, disjoint station sublayers only when multiple station classes are present, and one labels toggle. Station markers and labels are constrained to stations attached to currently visible centerline shots.
- Focused QTest coverage exists for loader behavior, camera math, scene statistics, projection, inspector state/widget, layer model, viewport widget loading, and viewport controller behavior.

## Current Boundaries To Preserve

- Treat the 3D viewer as a read-only projection over generated `.lox` artifacts. It must not participate in source transactions, Raw/Blocks/Map synchronization, or project validation diagnostics.
- Keep file loading and model/statistics logic in `src/core/ThreeDViewer*`; Qt Quick and widget code should consume prepared scene/model/controller state.
- Keep graphics-backend work isolated to the viewport item/host layer. Loader, camera, scene model, and scene statistics should remain backend-independent.
- Keep user workflow changes documented in `docs/USER_MANUAL.md`; renderer-only or test-only changes do not need user manual edits.

## Goals

- Keep the viewer read-only and explicitly separated from `.th`, `.th2`, and `thconfig` source mutation.
- Preserve the C++ ownership boundary for loading, parsing, scene-model construction, camera state, and file/process interaction.
- Keep Qt Quick scoped to presentation, input, HUD rendering, and scene-graph drawing.
- Improve practical usability on real caves before adding broad format support.
- Keep the feature cross-platform: macOS, Windows, and Linux must remain viable graphics/deployment targets.

## Non-Goals

- Do not port the old Loch wxWidgets UI into Therion Studio.
- Do not add live 3D editing or source mutation from the viewer.
- Do not move Therion execution, file IO, parsing, or project indexing into QML.
- Do not bind the rest of the application to a new QML architecture as part of this work.
- Do not add `.3d` or `.plt` import until the `.lox` path is stable and there is a concrete compatibility need.
- Do not add speculative rendering abstractions until profiling shows a real bottleneck or platform limitation.

## Architecture Guardrails

- `src/core/ThreeDViewer*` owns format loading, neutral model data, scene statistics, and camera math that can be tested without widgets.
- `src/app/three_d_viewer/` owns the read-only shell, inspector state, layer model, projection helper, viewport controller, and Qt Quick host.
- QML files under `resources/qml/three_d_viewer/` should remain thin presentation surfaces backed by C++ state.
- The viewer shall not call `DocumentFile` or mutate open source documents.
- Production dependencies should continue to be composed at tab/main-window boundaries rather than hidden behind UI constructors.
- User-visible strings must remain translatable and documented in `docs/USER_MANUAL.md` when workflows change.

## Remaining Task Backlog

### 1. Rendering Correctness And Visual Quality

- Compare centerline, mesh, surface, station, label, altitude legend, compass, view-angle indicator, scale bar, and bounding-box rendering against Loch on a small set of known `.lox` files.
- Add targeted fixes for visual mismatches that affect navigation or interpretation, not cosmetic Loch parity for its own sake.
- Continue improving station marker and fully qualified label legibility beyond the current automatic overlap suppression, including priority ranking if dense real projects still hide important stations.
- Evaluate station marker rendering for dense caves and keep hover/picking usable when stations and labels are hidden or decluttered.
- Decide whether terrain surface rendering should stay enabled by default when large or visually dominant.

Next slices:

- Add a small visual-reference matrix document or fixture list that names the `.lox` files used for Loch comparison and what each file covers.
- Add one focused renderer/controller regression for a real mismatch before making visual changes; avoid broad renderer cleanup without a failing case.
- If dense labels hide important stations, add a priority policy in inspector/viewport state first, then tune rendering.

### 2. Performance And Scalability

- Profile the scene-graph renderer on small, medium, and large `.lox` files.
- Track frame time, load time, memory footprint, node count, vertex count, and label count.
- Batch or cache static geometry where profiling shows repeated scene-graph rebuild cost.
- Add spatial or screen-space indexing for station hover and measurement picking if dense projects make linear picking visibly slow.
- Extend label level-of-detail only if automatic overlap suppression is insufficient on large scenes.
- Keep GPU-dependent performance tests separate from fast core/service tests.

Next slices:

- Add lightweight renderer diagnostics for node count, station count, label count, and mesh vertex count when a `.lox` file loads in debug/log mode.
- Profile station hover/measurement picking before adding spatial indexing.
- Batch or cache only the first measured bottleneck; do not introduce a renderer abstraction layer preemptively.

### 3. Data Semantics And Loader Coverage

- Extend `.lox` fixture coverage beyond one sample cave, including:
  - nested survey namespaces,
  - surface shots,
  - splay shots,
  - duplicate shots,
  - hidden shots,
  - stations with entrance/fixed/continuation flags,
  - meshes without centerline coverage,
  - terrain surfaces and surface bitmap chunks.
- Add assertions for station fully qualified names, survey titles, shot flags, and scene statistics on those fixtures.
- The current sample `.lox` fixture matrix covers nested survey hierarchy, mesh groups, station flags, and surface/duplicate/splay shot flags. Terrain surface chunks are covered by a synthetic loader fixture; a real Therion-exported terrain fixture is still missing.
- Decide whether surface bitmap chunks should be represented in the scene model or explicitly ignored with documented rationale.
- Keep malformed-input coverage strict and ensure loader errors remain actionable.

Next slices:

- Add one real Therion-exported terrain fixture before changing surface rendering defaults.
- Add a loader test that documents surface bitmap handling, even if the decision is to ignore bitmap chunks initially.
- Keep synthetic malformed fixtures small and separate from real sample caves.

### 4. Project Workflow Integration

- Add a project-aware way to find or open generated `.lox` artifacts from the active project once the runner/artifact workflow is stable.
- Report missing, stale, unsupported, or failed `.lox` artifacts with actionable user-facing messages.
- Avoid implicit compile/run side effects from opening the viewer; any compile integration should remain an explicit project action.
- Consider optional camera/layer/coloring persistence only after the read-only workflow is stable and the persistence key is clear.

Next slices:

- Define the generated-artifact lookup contract first: selected config, expected output folder, stale-file signal, and user-facing action text.
- Add artifact discovery in a service or project workflow controller, not in `ThreeDViewerTab`.
- Keep "compile then open" as an explicit command with visible status; opening an existing `.lox` should remain side-effect free.

### 5. Cross-Platform Graphics And Packaging

- Verify the Qt Quick scene-graph viewport on macOS, Windows, and Linux with packaged builds, not only local developer runs.
- Confirm deployment includes required Qt Quick/QML modules and resources.
- Check behavior under different graphics backends where Qt selects Metal, Direct3D, OpenGL, or software fallbacks.
- Keep any backend-specific workaround isolated in the viewport host or renderer, not in core scene/model code.

Next slices:

- Add a packaging checklist entry for required Qt Quick/QML modules and resources before changing installer/AppImage/DMG wiring.
- For backend-specific defects, record platform, Qt version, graphics API, and fallback behavior in the plan or worklog before adding a workaround.

### 6. Accessibility And UI Polish

- Add or verify keyboard-accessible toolbar controls and focus behavior for the 3D viewer tab.
- Keep dark/light application chrome coherent while the 3D canvas remains Loch-style black.
- Ensure inspector controls remain usable in narrow sidebar widths.
- Keep hover and measurement cards readable for long fully qualified station names.
- Review translatable UI strings whenever labels, tooltips, or errors change.

### 7. Future Compatibility

- Leave Compass `.plt` and Survex `.3d` import as future compatibility work until there is a concrete user need.
- If future formats are added, convert them into the same `ThreeDViewerSceneModel` rather than adding format-specific render paths.
- Avoid importing old Loch runtime data structures directly; treat them as format/behavior reference only.

## Recommended Next Slices

1. Add or generate a real Therion-exported `.lox` fixture that contains terrain surface chunks and, if available, surface bitmap chunks.
2. Add debug/log diagnostics for 3D load/render statistics so later performance reports have comparable numbers.
3. Profile automatic station marker and label decluttering on dense real projects and add priority ranking only if important stations are hidden.
4. Profile the current Qt Quick scene-graph renderer on a large real cave and record the first concrete bottleneck before refactoring.
5. Specify generated-artifact discovery before implementing project-aware open-from-runner behavior.

## Verification Strategy

- Run `python3 scripts/check_structure_constraints.py` before each 3D-viewer change.
- Run `TherionCoreQTests` for loader, scene statistics, and camera/model changes.
- Run `ThreeDViewerQTests` for viewport shell, inspector, layer model, projection, and controller changes.
- For renderer or QML changes, build `therion_app` and manually open representative `.lox` files.
- For packaging or graphics-backend changes, verify at least one packaged build per supported platform before considering the task complete.
