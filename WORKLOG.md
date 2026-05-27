# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- P0 - Phase 9: continue map-editor parity polish, focusing on remaining interaction gaps, inspector ergonomics, and unresolved area/line ownership rewrite semantics.
- P1 - Phase 10: continue interactive drawing/insertion polish, focusing on Apple Pencil/freehand performance and remaining insertion edge cases.
- P2 - Phase 11: continue structured block-canvas authoring coverage and BlockEditor extraction/refactor slices.
- P1 - Block-canvas drag-loop crash hardening on macOS: guard drag-preview/drop callbacks during TextEditorTab teardown and verify no stale callback path remains around `.th2` + `.xvi` workflows.
- P1 - Block editor logical-line continuation support: treat trailing `\` command continuations as one command block across parsing/details/apply/configure/delete paths and verify with UI regression coverage.
- P1 - Map canvas zoom-level rendering balance: keep zoomed-out overview readable by reducing dominant geometry and `.xvi` stroke/handle visual weight without hurting editability at normal zoom.
- P1 - Map line-style simplification: remove legacy secondary line-detail overlay (`detailWidth`/`detailColor`) and keep a single-stroke line style model.
- P1 - Style schema naming cleanup: use consistent `stroke_width` keys for point and line widths in map style JSON (no legacy aliases).
- P1 - Style schema naming cleanup: use `stroke_style` instead of `pen_style` for line/area stroke style keys in map style JSON (no legacy aliases).
- P1 - Style schema naming cleanup: use `raw_type` + `raw_subtype` selector naming consistently in map style JSON.
- P1 - Area fill-pattern renderer tuning: continue post-vectorization tuning of `fill_pattern` readability across zoom (LOD-aware spacing/width/radius/opacity shaping, especially for `cross_hatch` density at high zoom).
- P1 - Area style catalog coverage: refine per-`raw_type` area styles in split `resources/map_object_styles/*.json` files for all generated Therion area types, keeping pattern density readable across zoom.
- P1 - Split map-style catalog: keep bundled and user map-style files on the same partial JSON convention, loading bundled `resources/map_object_styles/*.json` first via a scoped CMake resource glob and application-data overrides second.
- P1 - User map-style overrides: load partial JSON override files from the application data `map_object_styles` directory after bundled defaults, with default/type/subtype precedence and no project-level override layer.
- P1 - Windows installer Qt runtime layout: install `TherionStudio.exe` under `bin/` so deployed Qt DLLs and plugin directories, including `platforms/qwindows.dll`, stay relative to the executable.
- P1 - Windows GUI subsystem: link `TherionStudio.exe` without a console window so closing a console host cannot terminate the app.
- P1 - Main-window startup geometry: enforce usable default/minimum main-window size and clamp restored session geometry to avoid narrow Windows launches while preserving half-screen resizing.
- P2 - GitHub Actions runtime maintenance: keep Windows installer workflow actions on Node 24-compatible versions to avoid Node 20 deprecation warnings.
- P1 - Inspector type dropdown hydration: ensure `point`/`line`/`area` `Type` select values load from command catalog reliably across catalog schema variants (`commands` array/object), then validate map selection-panel editing workflow.
- P1 - Selection-panel editable type/subtype combos: keep popup suggestions unfiltered by current text so users can switch away from parsed value without clearing the field first.
- P2 - Phase 7: finalize UX/accessibility/platform-convention parity, with focus on shortcuts and keyboard behavior consistency on macOS, Windows, and Linux.
- P3 - Phase 8: continue release-readiness and packaging hardening, with focus on signing/notarization expectations, release reproducibility, and deferred Linux packaging strategy.

## Next Up

- P0: Continue Phase 9 with remaining map-editor parity polish in selection/details ergonomics and cross-platform input edge cases.
- P1: Continue Phase 10 with Apple Pencil/freehand stroke UX, broader parsed-document snapshot use, less-aggressive shape-sensitive bezier simplification, and point/line/area workflow polish.
- P2: Continue Phase 11 with object-reference target picking/resolution shortcuts, then selection/details glue consolidation without behavior changes.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Pen/touch navigation depends on automatic input-policy behavior; Sidecar and touch-screen edge cases remain a product risk.
- Map/text undo arbitration still depends on choosing between map `QUndoStack` and embedded text-editor undo until unified command-stack refactor.
- Packaging/signing/distribution requirements are not yet exercised end-to-end on all target platforms.

## Phase Plan

### Phase 7 - UX, Accessibility, and Platform Conventions (`MVP` baseline)

- Status: in progress.
- Remaining work: improve shortcut matrix, menu behavior, focus traversal, high-DPI behavior, dark/light palette transitions, and platform-native expectations.

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Status: in progress.
- Remaining work: harden release process and artifacts after the current Windows-installer + Homebrew flow, including signing/notarization expectations and deferred Linux packaging strategy.
- ~~Regenerated bundled app icons (`resources/app/TherionStudio.icns`, `resources/app/TherionStudio.ico`) from `resources/app/app-icon.svg` for the current branding iteration.~~
- ~~Switched runtime app-window icon source from SVG to clipped PNG (`resources/app/app-icon.png`) to avoid Qt SVG `clipPath` rendering artifacts in UI icon surfaces.~~
- ~~Regenerated bundled app icons (`resources/app/TherionStudio.icns`, `resources/app/TherionStudio.ico`) from externally refined `resources/app/app-icon.png` master artwork.~~

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Status: in progress.
- Remaining work: continue interaction parity review for inspector ergonomics, command-surface consistency, selection edge cases, and cross-platform input behavior.
- ~~Open validation item: confirm robust source rewrite behavior for `Split Here` on open area-referenced borders across mixed area-body layouts; closed-line split remains unsupported.~~
- ~~Fix `.xvi` background handling: do not treat `.xvi` as raster bitmap (proper scaling path + gamma disabled).~~
- ~~Fix `.xvi` metadata-load regression causing UI freeze/spinning ball via unbounded offscreen render and mismatched model bounds.~~
- ~~Improve `.xvi` visual fidelity with zoom-aware supersampled re-render so overlay stays sharp during map zoom.~~
- ~~Switch `.xvi` background rendering from pre-rasterized pixmap overlays to a vector-painted scene item path (grid/shots/sketch) to keep zoom quality and reduce re-raster cost.~~
- ~~Fix `.xvi` open-time UI hangs by caching parsed `.xvi` payloads and skipping geometry rebuilds when projection inputs are unchanged.~~
- ~~Differentiate `.xvi` shot classes visually: render splays with distinct color from traverse shots and keep sketches visually separate.~~
- ~~Tune `.xvi` rendering performance/clarity: faster station lookup for shot classification and LOD-aware drawing (hide splays/sketch/grid at distant zoom), plus slightly stronger line weights.~~
- ~~Preserve and render `.xvi` sketch-line color tokens (`connect` as dotted gray, named colors mapped directly) so auxiliary sketch semantics are visible in map background overlays.~~
- ~~Inspector smooth/control toggles now preserve line-vertex targeting even when scene selection temporarily drops during UI interaction (fallback to stored selected line/vertex).~~
- ~~Inspector `<<` / `>>` control-handle toggles now resolve through line-control owner vertices (selection/controller/details mapping), so closed-line vertices can reliably enable both handles for full smooth editing.~~
- ~~Inspector smooth toggle now preserves vertex selection across asynchronous scene refresh (flush + multi-attempt owner-vertex selection recovery).~~
- ~~Map render style defaults are now data-driven from `resources/map_object_styles/*.json` (point radius/outline, line thickness/detail/style, area thickness/fill-opacity/style), with safe hardcoded fallback when JSON is missing/invalid.~~
- ~~Extended map object style catalog to support per-type/per-subtype overrides with colors, dash patterns, and area fill patterns (`solid`/`hatch`/`dots`) from `resources/map_object_styles/*.json`, keeping defaults as fallback.~~

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Status: in progress.
- Remaining work: improve Apple Pencil/freehand stroke preview/output, solid freehand draft preview, less-aggressive shape-sensitive bezier simplification, and remaining insertion edge cases.
- ~~Line draft closure UX: clicking the first anchor in line mode now commits as closed line (`-close on`) without adding a duplicate terminal anchor row.~~
- ~~Closed-line smoothing parity: map editor now supports editing/rendering control handles for the closing last->first segment (including persisted closing Bezier row rewrite and parsed-model normalization without duplicate terminal anchor).~~

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Status: in progress.
- Remaining work: expand configurable block coverage, add target-picking/navigation affordances for map object references, improve insertion anchoring/source-safety feedback, and continue BlockEditor extraction.

## Refactor Tracks

- Track A: keep `TextEditorTab` as a thin orchestration shell; continue narrowing raw completion/catalog services and remaining friend/controller boundaries.
- Track B: continue extracting `MapEditorTab` responsibilities into focused controllers for drawing, inspectors, selection details, scene lifecycle, and undo/snapshot orchestration.
- Track C: consolidate shared source-edit/rewrite primitives used by Raw, Blocks, and Map modes into focused non-UI services.
- MainWindow maintenance: split status-bar/status-lifecycle methods into `src/app/MainWindowStatusUi.cpp` to keep `src/app/MainWindow.cpp` within structure-constraint line limits without behavior changes.
- BlockEditor next slice: consolidate selection/details glue (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, and related thin delegates) if API boundaries remain stable.
- Guardrail: each slice should own one responsibility, keep behavior stable, and update documentation when user-visible behavior changes.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured block changes.
- TH2 Visual grid fallback for documents without parseable geometry or valid source bounds, such as user-defined grid origin/extent.
- Broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Expanded rewrite corpus/regression coverage beyond the MVP fixture set.
- Extend `input` relative-path autocomplete semantics to other path-taking Therion commands/options, such as `-sketch`.
- Completion ranking polish for complex contexts so highest-confidence context-valid tokens stay first and low-signal fallback entries are reduced.
- Future Smart Trace implementation with real trace detection, preview, bezier simplification, and one undo step per accepted trace.
