# Unified Source DOM Plan

This plan tracks the long-running migration toward one shared, lossless Therion source model for `.th`, `.th2`, and `thconfig` files. Stable ownership and dependency rules remain in `ARCHITECTURE.md`; active operational priorities remain in `WORKLOG.md`.

## Phase Plan

- Phase 1 - Source snapshot foundation: preserve lines, comments, blanks, indentation, newlines, encoding, source type, offsets, token spans, and revision identity.
- Phase 2 - Logical command layer: group continuations, classify block-body rows, record block ranges, and expose command/argument/option source ranges.
- Phase 3 - Catalog-aware metadata: provide command domains, aliases, arity, allowed values, arguments, and document-type applicability from metadata.
- Phase 4 - Validator migration: make `TherionSourceValidator` consume the shared projection while preserving ranges, severity policy, and false-positive coverage.
- Phase 5 - Raw editor migration: drive highlighting, context help, completion, tooltips, and underlines from shared revision-keyed source data.
- Phase 6 - Blocks projection migration: read Blocks cards, nesting, references, fixed root commands, data bodies, and details from the shared projection.
- Phase 7 - Map/TH2 projection migration: read TH2 objects, references, Bezier geometry, backgrounds, and Smart Area insertions from the shared projection.
- Phase 8 - Structure/project index migration: feed Structure, validation, search, namespaces, root-config inference, and diagnostics from cached DOM snapshots.
- Phase 9 - Transaction and cache model: centralize source mutations, undo labels, revision checks, dirty state, projection invalidation, and selection restore.
- Phase 10 - Legacy removal gates: remove editor-local parsers and serializers only after consumers migrate and regression coverage exists.

## Current Implementation Baseline

- `TherionSourceText` preserves physical source lines, line endings, absolute line spans, and offset-to-line mapping for targeted source edits.
- `TherionSourceDocument` owns the lossless source-text snapshot, parsed physical lines, source metadata, block stack state, block ranges, source line roles, line-number lookup, offset lookup, and round-trip text reconstruction.
- `TherionSourceLogicalDocument` owns continuation grouping, logical command ranges, physical token/argument/option ranges, block context, catalog-aware command metadata, physical-line lookup, absolute-offset command lookup, and absolute-offset token lookup.
- `TherionCommandLineModel`, `TherionTokenRules`, and `TherionStringUtils` are the current shared command/token/string rule seams.
- `TherionSourceValidator`, `ProjectStructureIndex`, Raw completion/help/highlighting paths, and several Blocks/Map helpers already consume shared command/source parsing in focused areas.
- `TextEditorSourceTransactionController` is the current central source-transaction seam for source snapshots, undo labels, revision checks, projection invalidation, and selection restoration hooks.
- Map editor source writes are partially routed through `applySourceTextChangeWithSnapshot`, but several projection/rewrite helpers still own local source-shape knowledge.
- `TherionSourceDocumentTest` and `TherionSourceLogicalDocumentTest` now run inside `TherionCoreQTests`, with QTest coverage for physical/logical source snapshot roles, ranges, metadata, continuation handling, and catalog metadata.
- `TherionSourceSnapshotCache` provides the first narrow revision-keyed cache for physical and logical source projections. It only caches snapshots with positive source revisions, and catalog-backed logical projections require an explicit catalog revision key.
- Raw context-help token lookup now reuses `TherionSourceSnapshotCache` keyed by the editor document revision instead of reparsing the full document for each help-token lookup.
- Raw completion scope analysis, cursor token context, and required-argument tooltip lookup now reuse the same revision-keyed source snapshot cache from `RawEditorCompletionController`.
- Raw validation tooltips and syntax-highlighter validation underlines now validate against cached physical/logical source projections instead of reparsing full document text in each consumer.
- Validation fix planning for unclosed map blocks and empty scrap objects now uses `TherionSourceDocument` line lookup helpers instead of local line scans.

## Current Consumer Inventory

### Shared DOM Consumers Already In Place

- `TherionSourceValidator` validates from shared physical/logical documents and has catalog-backed projection coverage in `TherionCoreQTests`.
- `ProjectStructureIndex` uses `TherionSourceSnapshotCache` and `TherionSourceLogicalDocument` for several file, namespace, survey, scrap, map, and reference passes.
- Raw editor completion, cursor token context, required-argument tooltips, context help, validation tooltips, and syntax-highlighter validation underlines use revision-keyed source snapshots in focused paths.
- Blocks canvas rebuild and document outline building already use `TherionSourceSnapshotCache` / `TherionSourceLogicalDocument` for broad command projection.
- Map inspector/object-details code uses shared logical documents for selected option reads in focused areas, but geometry/object projection is still mostly token-line based.

### Remaining Local Parsing And Lookup Hotspots

- `TherionDocumentEditor.cpp` still contains many rewrite planners that split lines, parse rows, infer insertion offsets, and preserve line endings locally. These are high-risk because they own source mutation semantics.
- `MapEditorTab::parsedLinesForCurrentDocument()` still caches `TherionDocumentParser::parseTokenLines()` as a TH2 token-line compatibility projection used by scene refresh, selection, inspector object contexts, object move/delete planning, line split planning, area/reference resolution, and background metadata workflows.
- `MapEditorSceneRefreshController`, `MapEditorSelectionController`, `MapEditorObjectDetailsLogic`, `MapEditorObjectDetailsEditController`, `MapEditorObjectDeletePlanner`, `MapEditorObjectMovePlanner`, `MapEditorLineSplitPlanner`, `MapEditorSourceReferenceResolver`, `MapEditorAreaReferenceResolver`, and `MapEditorBackgroundLayers` still contain direct `parseLine`, `parseTokenLines`, `splitTextLines`, or `detectedLineEnding` calls.
- Blocks details, delete, data-block, toolbox, line-build, and option-argument helpers still contain small local `parseLine` / `tokenizeLine` calls where the logical document does not yet expose the exact needed operation.
- Map geometry tests and map geometry feature parsing still exercise token-line compatibility inputs; this is useful guardrail coverage but also marks the migration boundary for a future TH2 projection.
- `TherionBackgroundMetadata` and map background workflows preserve XTherion/Mapiah metadata through local line edits. They should move only after map background round-trip coverage is explicit.

### Current Migration Rule

Prefer small consumer migrations that replace one local scan or lookup with `TherionSourceDocument` / `TherionSourceLogicalDocument` APIs while preserving existing behavior and tests. Avoid broad Map scene or source-rewrite migrations until Windows map-input feedback confirms the current release-stabilization fix.

## Remaining Slices

### Slice 2A - Shared Projection Cache Discipline

Goal: stop reparsing full documents for cursor movement, context help, completion, structure refresh, and unrelated inspector/UI updates.

- Expand `TherionSourceSnapshotCache` into Structure/Validation only after stale-projection behavior is covered.
- Keep cache ownership outside widgets; UI shells should request snapshots by document revision or explicit text input through a narrow collaborator.
- Add invalidation tests for text edits, undo/redo, document reload, source type changes, and catalog refresh.
- Do not add long-lived widget-owned caches without a revision key and explicit source type/catalog key.
- Next slices:
  - Add a narrow app/text-editor source snapshot access context that wraps document text, source type, revision, encoding, and catalog key.
  - Move one remaining Raw helper that still reparses by cursor position to `TherionSourceLogicalDocument::tokenAtOffset()`.
  - Add a regression that moving the cursor repeatedly does not rebuild snapshots when the document revision is unchanged.

### Slice 2B - Logical Cursor Mapping Consumers

Goal: make cursor-position consumers use shared logical offset lookup instead of rebuilding line/column scans.

- Start with Raw editor because it has lower selection/geometry risk than Map and already owns a source snapshot cache.
- Replace one cursor-token lookup path with `TherionSourceLogicalDocument::tokenAtOffset()` while preserving existing completion/context-help behavior.
- Keep tests focused on quoted tokens, option tokens, comments, continuation rows, and end-of-line positions.
- After Raw, migrate a single Blocks or Structure cursor/navigation helper if it can use the same API without source mutation changes.

### Slice 4 - Blocks Consumer Migration

Goal: make Blocks cards and details read command/source structure from the logical document.

- Replace local command/option scans in Blocks details, option args, and move planning with logical command ranges where source fidelity matters.
- Keep block move/rewrite behavior source-preserving and one undo transaction per user-visible action.
- Add regressions for nested blocks, fixed root commands, data bodies, comments, continuation rows, and move undo/redo.
- Next slices:
  - Replace one Blocks details `parseLine(logicalLine.text, logicalLine.startLine)` call with an existing logical command or a new focused logical-command helper.
  - Add logical-document support for the exact missing operation only if it removes repeated parsing in at least one consumer.
  - Keep data-block dialog rewrites separate from read-only projection migration.

### Slice 5 - Map/TH2 Projection Migration

Goal: reduce TH2 map parser drift by making Map scene objects consume shared command and option ranges.

- Migrate Map object discovery, option parsing, area/line reference resolution, and Smart Area insert planning to shared logical commands in small vertical slices.
- Keep geometry-specific parsing in map-focused types, but remove duplicated generic command/option token rules.
- Add round-trip and undo/redo coverage for line/area/point edits, background metadata, object delete/move, and inspector quick-field writes.
- Do not start with scene rebuild or selection/handles. Start with read-only inspector/reference helpers that already have focused tests.
- Next slices:
  - Convert one Map inspector option read that currently scans `parsedLinesForCurrentDocument()` to logical command ranges.
  - Convert one area/reference resolver lookup from token-line compatibility to `TherionSourceLogicalDocument` while preserving Therion namespace order.
  - Introduce a `Th2GeometryProjection` only after read-only option/reference migrations prove the shared logical input is sufficient.
  - Keep `MapEditorTab::parsedLinesForCurrentDocument()` as a compatibility adapter until geometry projection has its own tests.

### Slice 6 - Transaction Ownership Closure

Goal: make source mutation semantics uniform across Raw, Blocks, Map, inspector, validation fixes, and background workflows.

- Route remaining user-visible source mutations through `TextEditorSourceTransactionController` or an equivalent narrow successor.
- Require each source transaction to carry an undo label, expected revision when available, dirty-state behavior, projection invalidation policy, and selection/cursor restoration policy.
- Keep structure guardrails preventing direct map-editor source mutation bypasses.
- Next slices:
  - Audit `TherionDocumentEditor.cpp` rewrite planners and group them by read-only planning vs. source mutation.
  - Move one non-map text-editor rewrite caller to revision-aware transaction results if it is not already covered.
  - Defer map background metadata writes until background-specific round-trip coverage and Windows map-input feedback are both stable.

### Slice 7 - Structure, Project Index, And Diagnostics

Goal: feed orientation and validation surfaces from cached DOM snapshots instead of independent reparsing.

- Reuse cached logical documents for Structure, project indexing, namespace/reference resolution, search, and validation.
- Keep Therion namespace semantics exactly as documented in `docs/THERION_COMPATIBILITY.md`.
- Make live diagnostics debounced, cancellable, revision-keyed, and centralized in the Validation panel.
- Next slices:
  - Identify one `ProjectStructureIndex` pass that still rebuilds or rescans a logical document unnecessarily and route it through the existing per-run cache.
  - Add a regression for qualified reference order (`object@child.parent`) before changing namespace/reference consumers.
  - Keep UI-side Structure refresh changes separate from core project-index projection changes.

### Slice 8 - Legacy Removal Gates

Goal: delete duplicate parsing/rewrite code only after coverage and consumers have moved.

- Track remaining editor-local tokenizers, option parsers, line splitters, numeric classifiers, and source-range heuristics.
- Remove one legacy path at a time after a migrated consumer has regression coverage.
- Keep unknown valid directives, comments, formatting, encodings, and line endings round-trip safe.
- Do not delete `parseTokenLines` compatibility users until Map geometry and legacy tests have replacement coverage.
- Keep direct `parseLine` calls for synthetic command snippets, user-entered token fields, and tests where a full document snapshot would add no source fidelity.

## Recommended Next Slice Queue

1. Raw cursor-token consumer: use `TherionSourceLogicalDocument::tokenAtOffset()` in one existing Raw completion/context-help path and preserve quoted/comment/continuation behavior.
2. Blocks read-only detail consumer: replace one local `parseLine(logicalLine.text, logicalLine.startLine)` call with a logical-command range helper.
3. Map inspector read-only option consumer: migrate one tested option read from `parsedLinesForCurrentDocument()` to `TherionSourceLogicalDocument`.
4. ProjectStructureIndex cache cleanup: remove one repeated logical-document lookup or local rescan inside an existing cached project-index pass.
5. TH2 projection design slice: define the smallest `Th2GeometryProjection` input/output contract before moving scene rendering.

## Verification Gates

- Parser/projection coverage for each migrated consumer.
- Round-trip coverage for source-sensitive edits.
- Source-range coverage for diagnostics and mutations.
- Undo/redo coverage for user-visible source changes.
- Affected editor regression coverage for Raw, Blocks, Map, Structure, Validation, and compiler-facing workflows.
- Performance-sensitive migrations should include at least one test or diagnostic that proves repeated cursor/selection events reuse revision-keyed snapshots rather than reparsing unchanged text.
