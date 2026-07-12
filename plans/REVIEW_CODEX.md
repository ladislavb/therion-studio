# Codex Repository Review

Date: 2026-06-02

Scope: static review of repository structure, dead-code candidates, duplicate code, maintainability, performance, battery impact, and the parser/write architecture for `.th`, `.th2`, and `thconfig` documents.

This document is an active review record. Completed follow-ups should be removed from this file and tracked through code, tests, `ARCHITECTURE.md`, `WORKLOG.md`, or archive history as appropriate.

Status update: 2026-07-12

- Unified Source DOM migration M0-M9 is implemented and the completed plan is archived at
  `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md`.
- `TherionSourceText`, `TherionSourceDocument`, `TherionSourceLogicalDocument`, `TherionSourceSnapshotCache`, and
  `Th2GeometryProjection` provide the current shared source snapshot, logical command, and TH2 projection foundation.
- New direct `TherionDocumentParser` production call sites are guarded by `scripts/check_structure_constraints.py`.
- Map editor release stabilization has a separate Windows-input diagnostic/fix trail; do not fold broad map rendering
  rewrites into release-stabilization work without new evidence.
- This review should now be read as an architecture risk register. Detailed implementation slices belong in the focused plans and `WORKLOG.md`.

## Executive Summary

Therion Studio now has one authoritative shared source model for Therion documents. The largest remaining architecture risk
is no longer parser/model convergence, but keeping future source mutations, projection refreshes, and UI smoke workflows
inside the shared transaction/projection boundaries.

For `2026.7.2`, avoid broad parser or renderer rewrites. Keep current behavior stable, use the existing guardrails, and
treat source-model changes as focused maintenance or regression fixes.

## Priority Findings

### P0 - Unified Lossless Source Model Baseline

Evidence:

- `TherionSourceDocument`, `TherionSourceLogicalDocument`, and `Th2GeometryProjection` are the shared projection
  boundaries for Raw, Blocks, Map, Structure, Validation, completion/help, and source rewrite planning.
- Remaining direct parser calls are documented core/synthetic exceptions enforced by `scripts/check_structure_constraints.py`.
- The completed migration history is archived in `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md`.

Risk:

- Future changes can still regress if new UI or planner code bypasses source snapshots/projections or source transaction
  helpers.
- Compatibility fallback APIs can outlive their usefulness unless they remain guarded and tested.

Recommended direction:

- Keep raw text as the canonical persisted output.
- Extend shared source/projection types when new behavior needs command, token, block, option, geometry, background, or
  reference data.
- Keep legacy token-line compatibility APIs narrow, tested, and outside new production UI code where practical.
- Run structure guardrails and focused regression tests before treating source-model changes as complete.

### P0 - Source Writes Are Not Yet One Cross-Editor Transaction

Evidence:

- Map source mutations increasingly route through the shared source transaction controller, but Raw, Blocks, Map, inspector, and source rewrite workflows do not yet submit every user-visible source mutation through one required service.
- General source rewrites still involve `TherionDocumentEditor` rewrite methods and editor-specific controllers.
- Undo ownership is still split between map `QUndoStack` and embedded text-editor undo state.

Risk:

- Undo/redo behavior can vary by editor path.
- Snapshot creation, dirty-state updates, projection refreshes, selection restoration, and stale-revision handling can regress when new source mutation paths are added.
- Mixed map/text edits remain harder to reason about than one durable document transaction timeline.

Recommended direction:

- Continue expanding `TextEditorSourceTransactionController` or its successor until Raw, Blocks, Map, inspector, validation fixes, imports, normalization, and geometry/background edits submit through one required source-edit path.
- Every source mutation should submit a single request containing before/revision identity, target source range or replacement, undo label, dirty-state policy, projection refresh policy, and selection/cursor restoration intent.
- The service should own revision checks, replacement, undo snapshot creation, dirty-state updates, parsed-cache invalidation, and post-change projection refresh.
- Release-stabilization exceptions should stay narrow and tested. For example, map vertex moves now defer scene refresh after source transactions to avoid UI stalls; broader transaction redesign should remain separate.

### P1 - Large Multi-Responsibility Files

Largest active risk areas:

- `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp`
- `src/core/TherionDocumentEditor.cpp`
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp`
- `src/app/MainWindow.cpp`
- `src/app/text_editor/map_editor/MapEditorCanvasEditController.cpp`
- `src/app/text_editor/map_editor/MapEditorSceneItems.cpp`
- `src/core/ProjectStructureIndex.cpp`
- `src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp`
- `src/app/text_editor/map_editor/MapEditorSceneInternals.h`

Risk:

- These files still mix parsing, rendering, state coordination, source-write behavior, and UI concerns.
- Changes are harder to review and easier to regress.
- UI-layer business rules are more likely to creep back in.

Recommended splits:

- `TherionDocumentEditor.cpp`: source-range edits, object option rewrites, TH2 geometry rewrites, block rewrites, and append planners.
- `MapEditorSceneRenderer.cpp`: geometry projection, point rendering, line rendering, area rendering, labels, editable overlays, and station-label policy.
- `MapEditorBackgroundLayers.cpp`: metadata parser/writer, image cache/loading, placement model, inspector UI controller, and source rewrite adapter.
- `ProjectStructureIndex.cpp`: config resolver, input graph scanner, namespace resolver, tree builder, and diagnostics.

### P1 - Remaining Duplicate Interpretation Logic

Evidence:

- Command-line and option parsing are centralized in core for many paths, but option editing/serialization behavior still exists across Block details, Map object settings, and source rewriters.
- Some line-ending, source-range, and geometry interpretation logic still lives close to renderer, planner, or UI code.
- Renderers and inspectors still sometimes consume source text or token-line projections instead of already-parsed command/option data.

Risk:

- Syntax and option behavior can diverge between renderer, inspector, validator, structure index, and writer.
- Fixes can require coordinated edits in several layers.

Recommended direction:

- Keep renderers, inspectors, validators, and structure projections consuming parsed command/option/source-range data instead of retokenizing.
- Continue moving option serialization/editing behavior behind focused core command-line/source-model APIs.
- Prefer extending `TherionSourceText`, `TherionTokenRules`, `TherionCommandLineModel`, and future source document/projection types over adding editor-local helpers.

## Dead Code and Legacy Candidates

### `BlockEditorConfigureController`

Evidence:

- `src/app/text_editor/block_editor/BlockEditorConfigureController.cpp` appears scoped to data-row editing despite its broader name.
- `showCommandHelpOnly` is passed through but explicitly unused.

Recommendation:

- Rename/scope it to `BlockEditorDataRowsController` or fold it into the specific data-row action path.
- Remove the unused `showCommandHelpOnly` parameter unless a real caller needs it.

### Block Apply Naming

Evidence:

- `BlockEditorApplyExecutor` and `BlockEditorApplyStateController` still exist.
- The UI no longer has an explicit Apply button, but auto-commit paths still call apply-oriented code.

Recommendation:

- Treat this as stale naming, not dead code.
- Rename toward the current behavior, for example `BlockEditorSelectionAutoCommitController`, when touching the area.

### MainWindow Top-Level Files

Evidence:

- `src/app/` still has many `MainWindow*` and `TherionRunner*` files at top level.
- Some are proper shell/orchestration components, but this area is still large and active.

Recommendation:

- Do not move files casually.
- Continue extracting by workflow only when it reduces responsibility mixing and updates structure guardrails.

## Consolidation Opportunities

### Inspector Infrastructure

Keep moving Raw, Blocks, and Map inspector surfaces toward shared panel foundations:

- one File inspector implementation where practical,
- one Context Help implementation where practical,
- one tab/pane layout implementation,
- distinct content widgets only where the underlying workflow differs.

### Command Option Editing

The catalog-backed command option component should remain the shared option editor for:

- Map object settings,
- Block selected command options,
- future command option dialogs.

Avoid adding a second option parser or custom option table in Map or Blocks.

### Style Metadata vs Renderer Heuristics

Map style behavior should remain data-driven:

- guide spine visibility,
- editable line affordances,
- station label LOD,
- decoration visibility,
- dark/light rendering choices.

If a style needs editing-only guides, put the behavior in style metadata or a dedicated editor overlay policy, not hardcoded per type in renderer code.

## Performance and Battery Findings

### Full Document Parse and Full Scene Refresh

Evidence:

- Map scene refresh still clears/rebuilds broad scene state.
- Some parsed document caching exists, but derived map geometry, structure, background metadata, and source bounds projections are not yet consistently cached as independent revision-keyed products.
- Recent DOM work adds shared physical/logical snapshots and offset lookup APIs, but Map geometry and scene refresh still depend heavily on token-line compatibility projections.

Risk:

- Large `.th2` files can cause UI-thread work on text-driven refreshes.
- Full scene rebuilds are battery-expensive with raster backgrounds and labels.

Recommendations:

- Cache parsed document snapshots by document revision.
- Cache derived projections separately: map geometry, structure tree, background metadata, and source bounds.
- Debounce text-change driven map/structure refreshes.
- Avoid clearing/rebuilding the entire scene when a single object or line changed.
- Move expensive projection building to a worker where possible, then apply UI deltas on the main thread.

### Raster Background Recompute Guardrails

Evidence:

- Future background-layer changes can still accidentally recompute expensive raster images from UI-facing paths.

Recommendations:

- Do not recompute background images during unrelated theme, appearance, selection, or layer-order updates.
- Keep layer order in model state and render from that stable model.
- Preserve stale-result checks for asynchronous raster work.

### Deferred Retry Timers

Evidence:

- `MapEditorCanvasEditController` still uses delayed `QTimer::singleShot(...)` attempts after some source edits to restore scene selection once the scene has refreshed.

Risk:

- The retry sequence is timing-dependent and does repeated work whether or not the scene is already ready.
- It can still fail on slow machines or under heavy load if the scene refresh completes after the final retry.

Recommendation:

- Replace retry timers with an explicit scene refresh completion signal or callback.
- Selection restoration should run once after the relevant scene generation is available.
- Keep the recent deferred-refresh fix for vertex moves as a release stabilization measure, not as the final scene-refresh architecture.

### Icons and Small Pixmaps

Evidence:

- Lucide pixmap rendering appears in several UI areas.
- Some cache exists, but rendering factories are duplicated.

Recommendation:

- Keep one icon factory/cache service for Lucide pixmaps and action icons.
- Cache by icon name, color, size, device-pixel ratio, and enabled/disabled state.

### Structure Index

Evidence:

- `ProjectStructureIndex.cpp` parses inputs and builds a snapshot with config resolution, input graph, map/scrap references, and diagnostics.

Risk:

- Project-wide indexing can become expensive on large projects.

Recommendations:

- Keep indexing asynchronous/cancellable.
- Cache per-file parsed source snapshots by file path, mtime, and size.
- Rebuild only affected graph branches after a file save.
- Avoid running structure refresh on cursor/source-line movement when source content did not change.

## Parser Architecture Recommendation

Introduce these layers incrementally:

1. `TherionSourceText`: lossless physical source lines, newline preservation, line offsets, and safe line-range replacement. Initial implementation exists.
2. `TherionSourceDocument`: immutable parsed physical snapshot with document revision metadata, file type, encoding metadata, block state, line roles, and source ranges. Initial implementation exists.
3. `TherionSourceLogicalDocument`: continuation-aware logical command projection with argument/option/token ranges, catalog metadata, and cursor offset lookup. Initial implementation exists.
4. Projections: future `TherionBlockProjection`, `Th2GeometryProjection`, `TherionStructureProjection`, and `TherionSyntaxProjection` should build from the shared source/logical documents.
5. Source change service: continue converging on `TextEditorSourceTransactionController` or a narrow successor for line-range rewrite, text replacement, undo snapshot, parsed cache invalidation, and selection restoration.

Migration path:

1. Migrate low-risk Raw/Blocks/Validation/Structure consumers to the shared source/logical document APIs.
2. Move one tested Map inspector/reference read-only path from token-line compatibility to logical commands.
3. Define a standalone `Th2GeometryProjection` contract before moving geometry extraction out of `MapEditorSceneRenderer.cpp`.
4. Move project structure parsing from remaining token-line compatibility paths onto the shared source document/projection model.
5. Move all editor writes to a shared transaction service.
6. Delete or rename legacy wrappers once all call sites use the new service and regression coverage exists.

## Release-Safe Improvements

Before public release, prefer low-risk guardrails over broad rewrites:

- Add regression tests for remaining parser/token rules, including negative numbers, exponent notation, quoted strings, comments, comment-only lines, line-point option rows, and `revise ... -stations ...`.
- Add a smoke benchmark for opening a large `.th2` file and toggling dark/light mode.
- Keep structure constraints guarding against UI-side source mutation bypasses.
- For Windows map-input issues, use opt-in diagnostics and focused fixes from logs before starting broad Map/TH2 projection migrations.

## Suggested Verification Strategy

For each parser/source-write migration step:

- Unit-test round-trip behavior with real sample `.th`, `.th2`, and `thconfig` files.
- Assert that blank lines, comments, line endings, and encoding are preserved.
- Assert one undo item per user operation.
- Assert redo restores the same source text.
- Test Map, Block, and Raw projections from the same parsed snapshot.
- Run structure index tests against root `thconfig`, `thconfig.work`, multiple root configs, nested `input`, nested survey namespaces, and map/scrap references.

## Final Recommendation

For release, keep the current implementation stable and avoid major parser rewrites. The biggest actionable pre-release work is to keep guardrail tests around source writes and known parser edge cases.

For the next major development phase, make the unified lossless parser and shared source transaction service the central architecture objective. This will reduce duplicated code, improve undo/redo consistency, make Map/Block/Raw views coherent, and provide the best leverage for performance and battery improvements.
