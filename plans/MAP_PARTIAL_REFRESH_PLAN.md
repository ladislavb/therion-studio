# Map Partial Refresh Plan

Date: 2026-06-30

Scope: reduce post-edit map latency by refreshing only the affected TH2 geometry item group when it is safe, while keeping full scene refresh as the correctness fallback.

This plan is a Map scene/rendering performance plan. It should stay compatible with the shared source model by accepting
already-built geometry projections at narrow boundaries instead of baking document reparsing into renderer APIs.

## Current State

- Full map scene refresh reparses token lines, collects entries and geometry features, clears the map scene, renders every item, restores backgrounds/drafts/selection, reapplies presentation, and updates related UI.
- `MapEditorLargeScenePerformanceSmokeTest` provides a generated large-map harness around roughly 2k parsed lines and 6k scene items.
- Vertex selection restoration has an index through `mapVertexItemsByKey_`, which avoids scanning the full scene for many post-refresh selection restores.
- Simple unstyled open-line vertex commits can skip the full scene refresh and restore selection locally.
- Styled/decorated line commits still use full scene refresh because generated artefacts such as segment strokes, guide spines, line decorations, labels, direction ticks, handles, and connectors must stay consistent.
- Drag preview for styled line segments now updates `wall:blocks` guide paths during vertex movement, preventing stale preview geometry while the drag is active.
- Most scene items already carry `kMapSceneLineNumberRole`, but `mapItemsByLine_` stores only the primary item for a source line, not the full rendered item group.
- `renderMapWorkspaceScene()` owns the line rendering branch directly; there is no reusable public helper that renders one `MapGeometryFeature::Line` into an existing scene.

## Goals

- Avoid rebuilding the whole map scene after safe line vertex commits.
- Keep `line blocks`, slopes, decorated lines, line labels, direction ticks, Bezier controls, and guide spines visually correct after source commits.
- Preserve selection, hover/presentation state, inspector state, toolbar status, undo/redo behavior, and source transaction semantics.
- Keep partial refresh APIs ready to consume a future DOM-backed TH2 geometry projection.
- Keep full scene refresh as the fallback for ambiguous, cross-object, or area-dependent edits.

## Non-Goals

- Do not rewrite the Map source projection architecture in this plan.
- Do not remove `MapEditorTab::parsedLinesForCurrentDocument()` or token-line compatibility APIs.
- Do not rewrite the whole map renderer.
- Do not partial-refresh area fills, scrap clipping, referenced areas, backgrounds, drafts, cards, or Structure/Validation projections in the first implementation.
- Do not make partial refresh the default for every source change.
- Do not introduce a generic scene item registry that owns unrelated background/draft/card lifecycles.

## DOM Compatibility Boundary

Partial refresh should be shaped around projection inputs, not text parsing:

```cpp
refreshSingleGeometryFeature(const MapGeometryFeature &feature,
                             const MapGeometryRenderContext &context);
```

The first implementation may still obtain `MapGeometryFeature` from existing token-line helpers, but the refresh/render API should not require document text or line-number reparsing internally.

The current shared source model can provide the feature source through a cached TH2 geometry projection:

```text
TherionSourceDocument revision -> TH2 geometry projection -> MapGeometryFeature -> single-feature render
```

Avoid APIs shaped like:

```cpp
refreshLineByParsingWholeDocumentTextAgain(int lineNumber);
```

Those would make the performance fix harder to keep when Map projection moves to the shared source model.

## Safety Model

Partial refresh is safe only when replacing one source line's rendered geometry cannot invalidate other rendered objects.

Initial safe cases:

- One standalone line feature exists for the edited source line.
- The edit updates coordinates/controls of that line without changing its command kind, source line ownership, identifier, close state, subtype declarations, line-point option rows, or referenced objects.
- No rendered area depends on this line as a border/reference.
- The global source bounds used for scene projection are unchanged or the changed feature stays inside the previous source bounds.
- Background, draft, and card layers do not need restoration.

Fallback to full refresh when:

- The line has an identifier referenced by an area.
- The edit changes geometry enough to expand or shrink global source bounds.
- The edited feature is closed and participates in area clipping or fill behavior.
- The affected source change is not a simple coordinate/control rewrite.
- The current selection cannot be restored from the refreshed item group.
- Any required item group metadata is incomplete.

## Phase 1 - Item Group Ownership

Goal: make "all scene items for source line N" explicit enough to remove and replace a single rendered geometry group.

Slice 1A - Inventory and Guardrails

- Audit all line-rendered scene items and confirm every item has `kMapSceneLineNumberRole`.
- Identify items that should not be removed by geometry partial refresh, such as backgrounds, draft items, cards, and non-geometry overlays.
- Add a focused test or assertion helper that rendered `wall:blocks` lines expose all expected line-number metadata on primary path, segment paths, guide spines, decorations, handles, connectors, label, and direction tick.
- Keep this slice behavior-preserving.

Slice 1B - Geometry Item Group Removal Helper

- Add a focused helper that removes geometry scene items for one source line:
  - scans scene items for `kMapSceneLineNumberRole == lineNumber`
  - limits removal to geometry-owned items, for example `kMapItemRole == kMapItemGeometryValue`
  - removes the primary `mapItemsByLine_` entry
  - removes matching `mapVertexItemsByKey_` entries
  - deletes removed `QGraphicsItem`s safely after `scene->removeItem()`
- Keep background, draft, card, and unrelated overlay items untouched.
- Add tests with multiple line features and a point/area nearby to prove only the target line group is removed.

Slice 1C - Item Group Reindexing

- Ensure newly inserted handles are registered into `mapVertexItemsByKey_`.
- Ensure the primary rendered line item is reinserted into `mapItemsByLine_`.
- Add coverage that selection restore by indexed vertex still works after removing/readding one line group.

Verification:

- `MapGeometryFeatureParsingTest`
- `MapEditorCanvasEditSourceTransactionTest`
- `MapEditorDragUndoRedoSmokeTest`
- `MapEditorLargeScenePerformanceSmokeTest`
- `python3 scripts/check_structure_constraints.py`

## Phase 2 - Extract Single-Line Rendering

Goal: reuse the existing line rendering branch without requiring a full scene rebuild.

Slice 2A - Render Context Extraction

- Introduce a narrow render context for geometry rendering inputs currently captured in `renderMapWorkspaceScene()`:
  - scene
  - source bounds
  - preview bounds
  - map scale
  - style catalog or resolved style access
  - canvas theme
  - vertex/item indices
  - move/preview callbacks
  - orientation applicability callbacks where needed
- Keep this type map-renderer-local unless another module needs it.
- Do not move source parsing, document text, or selection logic into this context.

Slice 2B - Extract Line Feature Renderer

- Move the `MapGeometryFeature::Kind::Line` rendering branch into a helper that accepts one feature and the render context.
- Preserve:
  - segment-styled paths
  - line decorations
  - guide spines
  - line labels
  - direction tick
  - anchor handles
  - slope left handles
  - Bezier control handles/connectors
  - interactive preview callbacks
  - item metadata and z-values
- Keep the full-scene renderer calling the same helper for all line features.
- Add focused tests proving full-scene rendering and single-line rendering produce equivalent item metadata for a representative line.

Slice 2C - Keep Point/Area Out Of Scope

- Leave point and area rendering in the full renderer unless a later performance trace proves they need the same extraction.
- Document any temporary duplication introduced during extraction and remove it before widening partial refresh.

Verification:

- Existing line rendering tests must continue to pass.
- Add or update tests around `wall:blocks`, slope, Bezier controls, closed line, line label, and line-point metadata markers where practical.

## Phase 3 - Safe Partial Refresh Hook

Goal: use the item group remover and single-line renderer from source transaction projection invalidation hooks.

Slice 3A - Partial Refresh Eligibility

- Add a pure eligibility helper that receives:
  - before feature
  - after feature
  - current geometry feature set or a small dependency index
  - previous source bounds
  - new source bounds
- Return an explicit enum such as:
  - `Eligible`
  - `NeedsFullRefreshAreaDependency`
  - `NeedsFullRefreshBoundsChanged`
  - `NeedsFullRefreshFeatureShapeChanged`
  - `NeedsFullRefreshUnsupportedKind`
- Start with conservative line-only eligibility.
- Add tests for simple open line, `wall:blocks`, slope, closed line, area border dependency, and bounds expansion.

Slice 3B - Feature Lookup Boundary

- Add a narrow feature lookup function at the scene refresh/controller boundary.
- First implementation may use existing parsed-line compatibility helpers.
- Keep the API ready for a future cached TH2 geometry projection by returning `MapGeometryFeature` / dependency metadata rather than raw parsed lines.

Slice 3C - Projection Invalidation Hook

- Add a new source transaction invalidation hook for eligible line moves:
  - obtain after feature
  - remove existing line geometry item group
  - render the updated line feature
  - restore selected vertex or line
  - apply inspector object visibility
  - update geometry selection presentation
  - refresh object details/help/status only as needed
  - discard or avoid the pending full refresh only after the partial refresh succeeds
- If any step fails, leave the pending full refresh in place and run the existing full refresh path.
- Preserve undo/redo behavior by letting undo/redo trigger the same eligibility and refresh path only when safe.

Slice 3D - Diagnostic Timing

- Add optional diagnostic logging for partial refresh when troubleshooting logging is enabled:

```text
map-partial-refresh kind=line line=42 items_removed=18 items_added=18 eligible=1 remove_ms=0 render_ms=2 selection_ms=0 ui_ms=1 total_ms=4
```

- Keep log volume bounded; log one line per committed partial refresh, not per mouse move.
- Include fallback reason when eligibility fails only if diagnostic logging is enabled.

Verification:

- Compare `map-scene-refresh total_ms` against `map-partial-refresh total_ms` in `MapEditorLargeScenePerformanceSmokeTest`.
- Confirm styled/decorated line edits no longer need full scene refresh when eligibility passes.
- Confirm area-border edits still fall back to full refresh.

## Phase 4 - Dependency Index For Areas And References

Goal: widen eligibility without breaking area fills or referenced geometry.

Slice 4A - Area Border Dependency Detection

- Build a lightweight dependency index from current geometry features:
  - line identifier -> line source number
  - area source line -> referenced border line numbers
  - line source line -> dependent area line numbers
- Keep Therion namespace/reference semantics aligned with `docs/THERION_COMPATIBILITY.md`.
- Add tests for direct area border references and namespace-qualified references before using the index for refresh decisions.

Slice 4B - Partial Refresh Of Dependent Area Groups

- If a line border edit affects one or more areas, decide between:
  - full refresh fallback
  - remove/re-render line plus directly dependent area groups
- Do not implement dependent area partial refresh until single-line partial refresh is stable.
- Preserve clipping, area place/z-order, and selection behavior.

Slice 4C - Bounds-Aware Refresh

- If the changed line expands global source bounds, full refresh remains the initial fallback because all preview coordinates can shift.
- Later optimization may preserve projection transform or update all item transforms, but that is a separate slice.

Verification:

- Area reference resolver tests.
- Split area border smoke tests.
- Large scene performance smoke with at least one dependent area fixture.

## Phase 5 - DOM Projection Handoff

Goal: replace compatibility feature lookup with a cached DOM-backed TH2 projection when the Unified Source DOM plan reaches Map projection slices.

Slice 5A - Projection Contract

- Define the smallest `Th2GeometryProjection` output needed by map rendering:
  - geometry features
  - source bounds
  - line/area dependency metadata
  - source revision
  - optional feature lookup by source line
- Keep scene item rendering independent of how the projection was built.

Slice 5B - Cache Integration

- Feed partial refresh eligibility from a revision-keyed projection cache.
- Avoid reparsing unchanged text during cursor movement, selection updates, inspector refreshes, and repeated vertex commits.
- Keep cache ownership outside widgets where practical.

Slice 5C - Legacy Removal Gate

- Remove token-line compatibility feature lookup only after:
  - full scene rendering consumes the projection
  - partial refresh consumes the projection
  - selection and inspector consumers have replacement coverage
  - round-trip/source mutation tests still pass

Verification:

- Add performance-sensitive coverage showing repeated line refresh uses the same revision-keyed projection when source revision is unchanged.
- Keep broad parser and map geometry regression tests until compatibility APIs are fully retired.

## Recommended Slice Queue

1. Slice 1A: audit and test line item group metadata, especially `wall:blocks`.
2. Slice 1B: add safe one-line geometry item group removal and tests.
3. Slice 1C: ensure item and vertex indices rebuild correctly after one-line group replacement.
4. Slice 2A: extract a narrow line render context.
5. Slice 2B: extract single-line renderer and make full-scene rendering use it.
6. Slice 3A: add conservative partial-refresh eligibility helper with explicit fallback reasons.
7. Slice 3C: wire eligible line vertex commits to partial refresh with full-refresh fallback.
8. Slice 3D: add bounded diagnostic timings for partial refresh.
9. Slice 4A: add line-to-area dependency index before widening eligibility.
10. Slice 5A: align the API with future `Th2GeometryProjection`.

## Acceptance Criteria

- Simple unstyled open-line vertex commits keep the existing no-full-refresh fast path.
- Styled/decorated standalone line commits, including `wall:blocks`, can refresh only their item group when eligibility passes.
- Area-dependent line commits fall back to full scene refresh until dependent-area partial refresh is explicitly implemented.
- Selection and visible handle state remain correct after partial refresh, undo, and redo.
- `mapVertexItemsByKey_` and `mapItemsByLine_` do not retain stale item pointers after one-line replacement.
- Full scene refresh remains available and correct for every unsupported case.
- Troubleshooting logs can distinguish partial refresh from full refresh and report fallback reasons when enabled.
- The implementation can later consume a DOM-backed TH2 projection without changing map scene item ownership APIs.

## Verification Gates

- Run `python3 scripts/check_structure_constraints.py` before every implementation commit.
- Run focused unit/smoke tests for:
  - `MapGeometryFeatureParsingTest`
  - `MapEditorCanvasEditSourceTransactionTest`
  - `MapEditorDragUndoRedoSmokeTest`
  - `MapEditorLargeScenePerformanceSmokeTest`
  - area/reference tests when dependency eligibility changes
- Add at least one regression per newly eligible geometry style.
- For performance claims, compare diagnostic timings or smoke-test metrics before and after the slice.
- For source-mutation slices, verify undo/redo and selection restoration explicitly.

## Risk Areas

- Stale `QGraphicsItem *` pointers in `mapItemsByLine_`, `mapVertexItemsByKey_`, selection, hover, or inspector state.
- Accidental removal of background, draft, card, or non-target geometry items.
- Incorrect z-order when reinserted line groups are added after existing scene items.
- Source bounds changes that make one-line replacement visually inconsistent with the rest of the scene.
- Area/reference dependencies where another rendered object depends on the edited line.
- Duplicating renderer logic during extraction instead of making full render and partial render use the same helper.
- Binding partial refresh too tightly to legacy token-line parsing instead of projection inputs.
