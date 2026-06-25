# SVG Background Support Plan

Date: 2026-06-25

Scope: add SVG background-layer support to the TH2 Map editor while preserving existing raster, XVI, PocketTopo, XTherion, and Mapiah background behavior.

This plan is intentionally incremental. The first implementation should make Mapiah `format=svg` metadata round-trip safely and make SVG usable as a map reference layer. A later implementation may replace the first rendering path with a fully vector-backed scene item.

## Current State

- The background layer pipeline supports raster images through `QImage` / `QPixmap`.
- The background layer pipeline supports `.xvi` through dedicated parsed geometry and `MapEditorXviBackgroundItem`.
- PocketTopo `.txt` backgrounds are converted to generated `.xvi` files before insertion.
- Core background metadata parsing already recognizes Mapiah `##MAPIAH## image_insert_v1` entries with `format=svg`.
- Mapiah metadata parsing already reads `filename`, `xx`, `yy`, `xScale`, `yScale`, `rotationCenterDx`, `rotationCenterDy`, `rotationDeg`, and `pivotSet`.
- Mapiah metadata writing currently serializes only `format=xvi` or `format=raster`; SVG would currently be written incorrectly as raster.
- The user manual currently states that Mapiah `format=svg` layers are not supported yet.

## Product Goal

Users shall be able to add an SVG file as a map background reference, see it in the Map editor, transform it with the same position, scale, rotation, pivot, visibility, and opacity controls as other compatible backgrounds, and preserve it as Mapiah `format=svg` metadata.

## Compatibility Goal

Therion Studio shall treat SVG backgrounds as Mapiah metadata, not as XTherion raster metadata. This preserves compatibility with Mapiah-authored files and avoids losing the source layer type if SVG rendering later becomes fully vector-backed.

Expected metadata form:

```text
##MAPIAH## image_insert_v1 {format=svg;filename=background.svg;xx=0;yy=0;xScale=1;yScale=1;rotationCenterDx=0;rotationCenterDy=0;rotationDeg=0;pivotSet=false}
```

## Non-Goals

- Do not add a new external dependency beyond Qt.
- Do not convert SVG files to `.xvi`.
- Do not write SVG layers as XTherion `xth_me_image_insert` metadata.
- Do not silently rasterize and rewrite SVG as a separate generated PNG file.
- Do not implement SVG editing; SVG is a read-only reference layer.
- Do not support embedded scripting or external network resources from SVG files.

## Phase 1 - Safe Metadata and UX Recognition

1. Extend Mapiah metadata writing so `TherionBackgroundLayerFormat::Svg` serializes as `format=svg`.
2. Keep `format=xvi` behavior unchanged, including `xviRoot`.
3. Keep `format=raster` behavior unchanged for normal raster images.
4. Update background file picker filters to include `*.svg`.
5. Update layer labels and inspector behavior so SVG is identified as SVG, not raster.
6. Disable Gamma controls for SVG layers unless a later rendering implementation provides a clear and documented color-adjustment model.
7. Update `SPECIFICATION.md` and `docs/USER_MANUAL.md` to replace the current "not supported yet" wording with the intended behavior.

Verification:

- Core or app-service test for `format=svg` parse/write behavior.
- Manual check that existing raster and XVI metadata still write exactly as before.
- Localization check if any visible UI text changes.

## Phase 2 - MVP Rendering Path

Recommended first rendering path: rasterize SVG into a high-resolution `QImage` / `QPixmap` and reuse the existing raster-like placement pipeline while preserving `layerFormat=Svg`.

Implementation notes:

- Add focused SVG loading/rendering logic, for example `MapEditorSvgBackgroundImage`.
- Use `QSvgRenderer` to validate and render SVG.
- Determine intrinsic SVG size from the renderer `viewBox` / `defaultSize`.
- Render to a capped high-resolution image similar to raster display-image handling, with a cap chosen to avoid excessive memory usage.
- Treat the rendered image as a display projection only; the source metadata and layer format remain SVG.
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

## Phase 3 - Vector-Backed Rendering

Optional later improvement: replace the MVP rasterized projection with a vector-backed `QGraphicsItem`.

Possible approaches:

- `QGraphicsSvgItem` for straightforward SVG display.
- Custom `QGraphicsItem` using `QSvgRenderer` when more control is needed over caching, bounds, opacity, and transform behavior.

Reasons to defer this:

- The current background item list and lifecycle are heavily `QGraphicsPixmapItem`-oriented.
- A vector-backed path should not force broad background-layer refactors during release stabilization.
- Rasterized SVG MVP is enough to validate metadata, UX, and transform semantics first.

Vector-phase requirements:

- Preserve sharp rendering during deep zoom.
- Preserve opacity, visibility, stacking order, pivot, scale, rotation, and Fit With Background behavior.
- Avoid excessive repaint cost for complex SVGs.
- Keep SVG loading safe: no network fetches, no script execution, and no user-invisible external dependency behavior.

## Architecture Guidance

- Keep metadata parsing and serialization in core/background metadata code, not in widgets.
- Keep SVG loading/rendering in a focused map-editor background module rather than adding more ad hoc branches to `MapEditorBackgroundLayers.cpp`.
- Do not add new source-write paths. SVG background insertion and transform changes should continue through the existing background source transaction path.
- Keep rendering projection separate from source metadata. An SVG may be rasterized for display without becoming a raster layer semantically.
- Use `TherionBackgroundLayerFormat::Svg` as the stable type boundary.

## UX Decisions

- File picker label should include SVG in the background layer list.
- The Backgrounds inspector should show SVG layers as transformable reference layers.
- Gamma should be disabled for SVG in Phase 1/2.
- Opacity should remain available.
- Rotation, scale, position, and pivot should remain available through Mapiah metadata.
- If SVG loading fails, the user should see an actionable warning or status message naming the file.

## Risk Areas

- SVG files can be very large or complex; cap rasterized display size and avoid blocking the UI thread for expensive loads.
- SVG intrinsic sizing can be ambiguous; define fallback behavior for missing width/height/viewBox.
- Existing code distinguishes primarily between `.xvi` and "not `.xvi`"; SVG support must avoid being accidentally treated as ordinary raster in metadata writes.
- Mapiah compatibility depends on preserving `format=svg` through every transform rewrite.
- Background layer item storage currently uses `QGraphicsPixmapItem *`; a later vector-backed implementation may require a narrow `MapEditorBackgroundLayerItem` abstraction.

## Acceptance Criteria

- Adding an SVG background creates Mapiah `format=svg` metadata.
- Reopening a TH2 document with Mapiah `format=svg` metadata restores the SVG background.
- Moving, scaling, rotating, pivoting, changing visibility, and changing opacity preserve `format=svg`.
- Existing raster, XVI, and PocketTopo background workflows continue to pass their current tests.
- The user manual documents SVG as a supported background layer after implementation.
- No SVG support code introduces a new non-Qt dependency.
