# SVG Background Support Plan

Date: 2026-06-25

Scope: add SVG background-layer support to the TH2 Map editor while preserving existing raster, XVI, PocketTopo, XTherion, and Mapiah background behavior.

This plan is intentionally incremental, but the implementation target is full Mapiah compatibility from the start. SVG layers should be stored as Mapiah `format=svg` metadata, carry Mapiah-compatible intrinsic size/viewBox metadata, and render as SVG layers rather than being rewritten or serialized as raster layers.

## Current State

- The background layer pipeline supports raster images through `QImage` / `QPixmap`.
- The background layer pipeline supports `.xvi` through dedicated parsed geometry and `MapEditorXviBackgroundItem`.
- PocketTopo `.txt` backgrounds are converted to generated `.xvi` files before insertion.
- Core background metadata parsing already recognizes Mapiah `##MAPIAH## image_insert_v1` entries with `format=svg`.
- Mapiah metadata parsing already reads `filename`, `xx`, `yy`, `xScale`, `yScale`, `rotationCenterDx`, `rotationCenterDy`, `rotationDeg`, and `pivotSet`.
- Mapiah SVG metadata additionally stores `intrinsicWidth`, `intrinsicHeight`, `sourceViewBoxLeft`, `sourceViewBoxTop`, `sourceViewBoxWidth`, and `sourceViewBoxHeight`.
- Mapiah metadata writing preserves `format=svg` and the SVG intrinsic size/viewBox fields.
- Existing Mapiah `format=svg` layers with intrinsic size and source viewBox metadata render as SVG background items in the map editor.
- Adding a new SVG layer from the Backgrounds file picker is wired and writes Mapiah `format=svg` metadata.

## Current Boundaries To Preserve

- SVG is a background reference layer, not an editable vector document.
- SVG source identity shall remain `TherionBackgroundLayerFormat::Svg` and Mapiah `format=svg`; it must not be rewritten as raster or XTherion image metadata.
- Metadata parsing/serialization belongs in core background metadata code. Rendering/loading belongs in focused map-editor background modules.
- Source writes for insertion and transform changes must keep using the existing background source transaction path.
- Expensive SVG validation/loading should not be introduced into unrelated selection, theme, or map-scene refresh paths.

## Product Goal

Users shall be able to add an SVG file as a map background reference, see it in the Map editor, transform it with the same position, scale, rotation, pivot, visibility, and opacity controls as other compatible backgrounds, and preserve it as Mapiah `format=svg` metadata.

## Compatibility Goal

Therion Studio shall treat SVG backgrounds as Mapiah metadata, not as XTherion raster metadata. This preserves compatibility with Mapiah-authored files and avoids losing the source layer type if SVG rendering later becomes fully vector-backed.

Expected metadata form:

```text
##MAPIAH## image_insert_v1 {format=svg;filename=background.svg;xx=0;yy=0;xScale=1;yScale=1;rotationCenterDx=0;rotationCenterDy=0;rotationDeg=0;pivotSet=false;intrinsicWidth=100;intrinsicHeight=80;sourceViewBoxLeft=0;sourceViewBoxTop=0;sourceViewBoxWidth=100;sourceViewBoxHeight=80}
```

## Non-Goals

- Do not add a new external dependency beyond Qt.
- Do not convert SVG files to `.xvi`.
- Do not write SVG layers as XTherion `xth_me_image_insert` metadata.
- Do not silently rasterize and rewrite SVG as a separate generated PNG file.
- Do not implement SVG editing; SVG is a read-only reference layer.
- Do not support embedded scripting or external network resources from SVG files.

## Phase 1 - Safe Metadata and UX Recognition

Done:

1. Extend Mapiah metadata writing so `TherionBackgroundLayerFormat::Svg` serializes as `format=svg`.
2. Keep `format=xvi` behavior unchanged, including `xviRoot`.
3. Keep `format=raster` behavior unchanged for normal raster images.
4. Parse and write SVG intrinsic metadata fields: `intrinsicWidth`, `intrinsicHeight`, `sourceViewBoxLeft`, `sourceViewBoxTop`, `sourceViewBoxWidth`, and `sourceViewBoxHeight`.
5. Update background file picker filters to include `*.svg`.

Remaining:

1. Confirm layer labels and inspector behavior identify SVG as SVG, not raster, in all layer lists and details rows.
2. Disable or hide Gamma controls for SVG layers; Mapiah SVG handling does not model raster gamma correction.
3. Update `SPECIFICATION.md` and `docs/USER_MANUAL.md` if current user-facing wording still says SVG is unsupported.

Verification:

- Core or app-service test for `format=svg` parse/write behavior, including intrinsic size/viewBox fields.
- Manual check that existing raster and XVI metadata still write exactly as before.
- Localization check if any visible UI text changes.

## Phase 2 - SVG Rendering Path

Current rendering path: validate and render SVG files with Qt SVG from a focused SVG background item. The map canvas uses full-viewport repainting so interactive point/line/area draft movement does not leave stale partial-repaint artefacts over SVG content.

Implementation notes:

- Add focused SVG loading/rendering logic, for example `MapEditorSvgBackgroundItem` and `MapEditorSvgBackgroundMetadata`. The initial `MapEditorSvgBackgroundItem` exists for Mapiah metadata restore and insertion.
- Use `QSvgRenderer` to validate and render SVG content.
- Determine intrinsic SVG size from explicit `width` / `height` and `viewBox`, matching Mapiah behavior where practical.
- If SVG metadata lacks width/height/viewBox, derive stable intrinsic data from the SVG file and persist it into Mapiah metadata on insertion. The initial insertion path supports explicit `width`/`height`, falls back to `viewBox` size, and synthesizes a `0 0 width height` source viewBox when width/height exist without a viewBox.
- When rendering an existing Mapiah SVG reference, use the stored intrinsic size/viewBox as the stable coordinate contract. If the source SVG lacks required root metadata, render with the stored metadata equivalent, matching Mapiah's normalization behavior.
- Use the same Mapiah placement transform fields as raster and XVI where applicable.
- Ensure Fit With Background includes SVG bounds.
- Ensure source-driven refresh reloads changed SVG files consistently with raster/XVI refresh behavior.

Verification:

- UI test that loads a TH2 document containing Mapiah `format=svg` metadata and confirms a background layer appears.
- Test that adding an SVG layer writes `format=svg`.
- Test that transform changes preserve `format=svg`.
- Manual checks with:
  - SVG with explicit width/height
  - SVG with only `viewBox`
  - SVG with transparency
  - SVG with a large canvas
  - SVG with relative path metadata

## Phase 3 - Background Layer Item Abstraction

The current background layer lifecycle is heavily `QGraphicsPixmapItem *`-oriented. A minimal SVG implementation may bridge this carefully, but the long-term direction should be a narrow background layer item abstraction shared by raster, XVI, and SVG layers.

Requirements:

- Preserve stable interactive repainting during point/line/area drawing.
- Preserve opacity, visibility, stacking order, pivot, scale, rotation, and Fit With Background behavior.
- Avoid excessive repaint cost for complex SVGs.
- Keep SVG loading safe: no network fetches, no script execution, and no user-invisible external dependency behavior.

Next slices:

- Introduce a narrow `MapEditorBackgroundLayerItem` abstraction only when a second renderer-specific branch needs the same lifecycle operation.
- Start with common operations that already exist for all formats: visibility, opacity, z-order, bounds, and placement transform.
- Do not migrate source metadata or inspector state into the graphics item abstraction.

## Architecture Guidance

- Keep metadata parsing and serialization in core/background metadata code, not in widgets.
- Keep SVG loading/rendering in a focused map-editor background module rather than adding more ad hoc branches to `MapEditorBackgroundLayers.cpp`.
- Do not add new source-write paths. SVG background insertion and transform changes should continue through the existing background source transaction path.
- Keep rendering projection separate from source metadata. SVG must never become a raster layer semantically.
- Use `TherionBackgroundLayerFormat::Svg` as the stable type boundary.
- Treat Mapiah's intrinsic size/viewBox metadata as part of the SVG background source contract.

## UX Decisions

- File picker label should include SVG in the background layer list.
- The Backgrounds inspector should show SVG layers as transformable reference layers.
- Gamma should be disabled for SVG.
- Opacity should remain available.
- Rotation, scale, position, and pivot should remain available through Mapiah metadata.
- If SVG loading fails, the user should see an actionable warning or status message naming the file.

Open checks:

- Verify whether existing UI labels/tooltips are already translatable and current for SVG.
- Verify whether failed SVG load is currently visible to the user or only logged/ignored.
- Verify whether Fit With Background uses SVG bounds after insertion and after reopening an existing Mapiah layer.

## Risk Areas

- SVG files can be very large or complex; avoid blocking the UI thread for expensive loads and avoid repainting unnecessarily.
- SVG intrinsic sizing can be ambiguous; define fallback behavior for missing width/height/viewBox.
- Existing code distinguishes primarily between `.xvi` and "not `.xvi`"; SVG support must avoid being accidentally treated as ordinary raster in metadata writes.
- Mapiah compatibility depends on preserving `format=svg` through every transform rewrite.
- Background layer item storage currently uses `QGraphicsPixmapItem *`; a later vector-backed implementation may require a narrow `MapEditorBackgroundLayerItem` abstraction.

## Acceptance Criteria

- Adding an SVG background creates Mapiah `format=svg` metadata.
- SVG metadata includes Mapiah-compatible intrinsic size and source viewBox fields.
- Reopening a TH2 document with Mapiah `format=svg` metadata restores the SVG background.
- Moving, scaling, rotating, pivoting, changing visibility, and changing opacity preserve `format=svg`.
- Existing raster, XVI, and PocketTopo background workflows continue to pass their current tests.
- The user manual documents SVG as a supported background layer after implementation.
- No SVG support code introduces a new non-Qt dependency.

## Recommended Next Slice Queue

1. Audit current SVG UI labels, inspector rows, Gamma control state, Fit With Background behavior, and failed-load reporting.
2. Add or update focused tests for transform rewrites preserving `format=svg` and intrinsic viewBox fields.
3. Update `SPECIFICATION.md` and `docs/USER_MANUAL.md` only after verifying actual UI behavior.
4. Add one UI/integration test that reopens a TH2 with Mapiah SVG metadata and confirms a background layer is restored.
5. Consider `MapEditorBackgroundLayerItem` only after the remaining SVG-specific branches make lifecycle duplication concrete.

## Verification Gates

- Run background metadata tests for every metadata parser/writer change.
- Run focused map background tests for insertion, reload, transform, visibility, opacity, and Fit With Background changes.
- Run `python3 scripts/check_structure_constraints.py`.
- Manually check one SVG with explicit width/height, one with only `viewBox`, and one with transparency.
