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
- `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` tracks the detailed Structure/Validation project-scan cache path,
  including shared project-index snapshots, per-file validation diagnostics cache, and later DOM-backed project projections.
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
- Map inspector/object-details, area-reference, selection-restore, line-extension start, scene refresh, Smart Area preview,
  background auto-adjust, object discovery, and selection navigation use shared logical commands or the TH2 geometry
  projection for production DOM-backed read-only paths, while source-rewrite planning still keeps explicit token-line
  compatibility boundaries.

### Remaining Local Parsing And Lookup Hotspots

- The remaining direct `TherionDocumentParser` production calls are quarantined by
  `scripts/check_structure_constraints.py` and limited to parser implementation, logical DOM construction,
  validator safe-fix/scope snippets, command-option value snippets, source-editor single-line rewrite helpers, and map
  pending-insert snippets that do not have source text yet.
- `TherionDocumentEditor.cpp` still owns low-level token rewrite helpers for source mutations, but their callers are
  routed through shared source snapshots and transaction boundaries. Any further extraction should preserve existing
  undo/redo and round-trip coverage.
- Compatibility tests still exercise token-line parsing APIs directly. This is retained as guardrail coverage for the
  parser core and source-preservation boundaries, not as a production UI parsing path.

### Current Migration Rule

Prefer small consumer migrations that replace one local scan or lookup with `TherionSourceDocument` / `TherionSourceLogicalDocument` APIs while preserving existing behavior and tests. Avoid broad Map scene or source-rewrite migrations until Windows map-input feedback confirms the current release-stabilization fix.

## 2026.7.2 DOM Completion Definition

The `2026.7.2` DOM migration is complete when the application has one authoritative source projection path for `.th`,
`.th2`, and `thconfig` documents, and editor/UI features consume that projection instead of owning duplicate parser
fragments. The low-level `TherionDocumentParser` may remain as a core implementation detail and for synthetic snippets,
but UI, project, validation, and map consumers should not rebuild independent full-document parse views.

Completion criteria:

- Raw, Blocks, Map, Structure, Validation, Project Search, and Compiler-facing navigation consume
  `TherionSourceDocument`, `TherionSourceLogicalDocument`, or focused projections derived from them.
- `MapEditorTab::parsedLinesForCurrentDocument()` is removed or reduced to a temporary compatibility adapter with no
  remaining production caller outside the TH2 projection boundary.
- `parseTokenLines()` is not used by production Map scene refresh or object/reference discovery once
  `Th2GeometryProjection` has equivalent coverage.
- Source mutations use `TextEditorSourceTransactionController`, `applySourceTextChangeWithSnapshot`, or a documented
  successor transaction service with undo label, expected revision, dirty-state update, projection invalidation, and
  selection/cursor restoration behavior.
- Legacy direct `parseLine`, `tokenizeLine`, `splitTextLines`, and line-ending heuristics are limited to core DOM
  construction, pure synthetic snippets, SQL/help/report non-Therion parsing, or tests that deliberately exercise legacy
  compatibility.
- Tests cover continuation lines, comments, blank lines, block bodies, raw `code ... endcode` bodies, TH2 point/line/area
  geometry, backgrounds, CRLF documents, undo/redo for source-changing actions, and project-index namespace/reference
  semantics.

Non-goals for `2026.7.2`:

- Do not rewrite the full parser grammar.
- Do not change Therion syntax behavior or namespace semantics as part of the migration.
- Do not add broad new product features while this is the active release theme.
- Do not delete compatibility APIs until their final production caller is migrated and regression coverage exists.

## Migration Operating Rules For Smaller Models

Use this section as the implementation contract for model runs with limited context.

- Work one slice per commit. Do not combine Raw, Blocks, Map, Structure, and transaction migrations in one change.
- Before editing, inspect current callers with `rg` and name the exact legacy call site being migrated.
- Prefer adding a narrow DOM helper over copying parser logic into another consumer.
- Add or update the nearest focused test before treating a migrated caller as done.
- Preserve behavior first. If the DOM exposes a slightly different interpretation, add coverage and make the difference
  explicit before switching the caller.
- Keep source-changing Map and background workflows behind explicit before/after source-text snapshots until the
  replacement transaction path has undo/redo and round-trip coverage.
- Update this plan or `WORKLOG.md` after each migrated slice so the remaining queue stays current.
- Run at minimum `python3 scripts/check_structure_constraints.py`, `git diff --check`, and the nearest affected test target
  before proposing a commit.

## Detailed Migration Queue

### M0 - Finish Current Blocks Parsed-Line Slice

Goal: close the already-started Blocks parsed-line helper migration before opening a new area.

Files:

- `src/app/text_editor/block_editor/BlockEditorSourceText.*`
- `src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp`
- `src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp`
- `tests/BlockEditorCanvasRebuildControllerTest.cpp`

Steps:

1. Keep `blockEditorParsedLineForLogicalLine()` as the shared helper for Blocks logical-line consumers.
2. Ensure it prefers `TherionSourceLogicalDocument` for logical commands, `TherionSourceDocument` for physical full-line
   comments and blank/source-role lines, then legacy parsing only as fallback.
3. Confirm no accidental dependency on a nonexistent `TherionSourceSnapshotCache::logicalDocument(sourceDocument)` API.
4. Keep the regression that checks both a physical full-line comment and a continued `team` command.

Verification:

- `cmake --build build-release --target BlockEditorCanvasRebuildControllerTest`
- `ctest --test-dir build-release -R BlockEditorCanvasRebuildControllerTest --output-on-failure`
- `cmake --build build-release --target TextEditorCaretInteractionTest`
- `ctest --test-dir build-release -R TextEditorCaretInteractionTest --output-on-failure`

Stop condition:

- Stop if Blocks details starts changing user-visible option rows or source rewrite behavior. That belongs to later
  Blocks slices.

### M1 - Inventory And Guardrails

Goal: produce a current, reviewed list of remaining legacy call sites and classify which are allowed to remain.

Suggested commands:

- `rg -n "TherionDocumentParser::parseLine|parseTokenLines|tokenizeLine|splitTextLines|detectedLineEnding" src tests`
- `rg -n "parsedLinesForCurrentDocument|TherionTokenLine|replaceTextForCommand|rewrite|SourceText" src/app src/core`

Steps:

1. Create a short table in this plan or a companion file under `plans/` listing each production call site.
2. Classify each call site as `core DOM construction`, `synthetic snippet`, `read-only consumer`, `rewrite planner`,
   `TH2 geometry compatibility`, or `non-Therion parsing`.
3. Mark `read-only consumer` entries as migration candidates.
4. Mark `rewrite planner` entries as transaction/round-trip candidates.
5. Mark `TH2 geometry compatibility` entries as blocked by `Th2GeometryProjection`.

Verification:

- No code behavior change is required.
- `python3 scripts/check_structure_constraints.py`

Stop condition:

- Do not start migrating callers in the same slice; keep the inventory reviewable.

### M2 - Shared Snapshot Access Context

Goal: make text-editor consumers request source/logical snapshots through one narrow context instead of each owning local
cache wiring.

Candidate files:

- `src/app/text_editor/TextEditorTab*`
- `src/app/text_editor/raw_editor/*`
- `src/app/text_editor/block_editor/*`

Steps:

1. Introduce a small text-editor source snapshot access type only if it replaces at least two duplicated local cache
   constructions.
2. Include text, source type, revision, encoding if available, and optional catalog key.
3. Keep ownership outside individual widgets where practical.
4. Migrate one existing Raw or Blocks consumer to the access type.
5. Add a regression or diagnostic that unchanged document revisions reuse the same cached projection where possible.

Verification:

- Nearest Raw/Blocks QTest target.
- `python3 scripts/check_structure_constraints.py`

Stop condition:

- Stop if the access type becomes a service locator with unrelated callbacks. Keep it source-projection-specific.

### M3 - Raw Cursor And Help Consumers

Goal: finish Raw read-only cursor consumers by using logical offset lookup.

Candidate files:

- `src/app/text_editor/raw_editor/RawEditorCompletionContextAnalyzer.cpp`
- `src/app/text_editor/raw_editor/RawEditorCompletionController.cpp`
- `src/app/text_editor/TextEditorContextHelpController.cpp`
- `src/editor/TherionSyntaxHighlighter.cpp`

Steps:

1. Pick one remaining Raw helper that maps cursor position to a token or command by local line scanning.
2. Replace it with `TherionSourceLogicalDocument::tokenAtOffset()` or `commandAtOffset()`.
3. Preserve behavior for quoted strings, option tokens, comments, continuation rows, and end-of-line cursor positions.
4. Add focused coverage in `TextEditorRawEditorQTests` or the existing nearest Raw test.
5. Repeat in later commits until Raw has no production full-document reparse for cursor movement, help lookup, completion,
   validation tooltip, or syntax-highlighting diagnostics.

Verification:

- `cmake --build build-release --target TextEditorRawEditorQTests`
- `ctest --test-dir build-release -R TextEditorRawEditorQTests --output-on-failure`

Stop condition:

- Do not change completion ranking, catalog metadata, or visible UI text in the same slice.

### M4 - Blocks Read-Only Consumers

Goal: make Blocks details, outline, toolbox, and read-only option presentation consume logical commands/ranges.

Candidate files:

- `src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp`
- `src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp`
- `src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp`
- `src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp`
- `src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp`

Steps:

1. Migrate one remaining `parseLine(logicalLine.text, logicalLine.startLine)` or `tokenizeLine(...)` call at a time.
2. Use `TherionSourceLogicalCommand::parsed`, token ranges, option ranges, or a new focused logical helper.
3. Add a helper to `TherionSourceLogicalDocument` only when one operation is needed by more than one Blocks consumer or
   removes repeated source-shape logic.
4. Add regressions for nested blocks, fixed root commands, data bodies, comments, and continued commands.
5. Keep source-changing Blocks operations separate unless the slice is explicitly a transaction slice.

Verification:

- `BlockEditorCanvasRebuildControllerTest`
- `TextEditorCaretInteractionTest`
- Any added Blocks-specific test target

Stop condition:

- Stop if a read-only migration requires changing block move/delete source rewrites. Split that into M8.

### M5 - Project, Structure, Search, And Validation Cache Closure

Goal: make project-level consumers share DOM-backed source snapshots without duplicate filesystem/source parsing.

Candidate files:

- `src/app/ProjectValidationScanner.cpp`
- `src/app/ProjectSourceProjectionCache.*`
- `src/core/ProjectStructureIndex.cpp`
- `src/app/ProjectSearchScanner.cpp`
- `src/app/ProjectScanCacheService.*`

Steps:

1. Follow `plans/PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` for cache-specific order.
2. Migrate `ProjectSearchScanner` from raw `text.split('\n')` only if source ranges and line numbers remain identical.
3. Remove one repeated logical-document lookup or local rescan inside `ProjectStructureIndex` per slice.
4. Add or update diagnostics/tests for cache hit/miss counters when repeated validation/search/structure requests use the
   same content.
5. Preserve namespace semantics from `docs/THERION_COMPATIBILITY.md`, especially `object@child.parent` order.

Verification:

- `TherionCoreQTests` for structure/index changes.
- Project validation/scanner tests for cache behavior.
- `python3 scripts/check_structure_constraints.py`

Stop condition:

- Stop if a change requires incremental dependency tracking across files. That is a separate architecture slice.

### M6 - TH2 Projection Contract

Goal: define the smallest typed DOM-backed projection for Map scene/object consumers before replacing
`parseTokenLines()` in production Map paths.

New type direction:

- Prefer names like `Th2GeometryProjection`, `Th2GeometryCommand`, `Th2PointObject`, `Th2LineObject`, `Th2AreaObject`,
  and `Th2BackgroundObject` under a focused core or map-editor projection boundary.
- The projection should take `TherionSourceDocument` / `TherionSourceLogicalDocument` input and expose typed TH2 objects
  with source ranges and stable identities.

Minimum projection data:

- Command kind: point, line, area, scrap, map, background-related metadata.
- Physical/logical source line range and absolute source range.
- Object id, type, subtype, options, comments/source preservation references.
- For line geometry: point rows, control-point markers, smooth/subtype flags, and original row ranges.
- For area geometry: border references and line ids.
- For backgrounds: image path token range, XTherion/Mapiah metadata lines, transform metadata, and original line ranges.

Steps:

1. Add projection tests first using representative TH2 snippets with comments, blank lines, CRLF, continued options, point
   objects, line blocks, area blocks, scrap blocks, and background metadata.
2. Build projection from logical commands and source line roles. Do not parse the full document through
   `parseTokenLines()` inside the new projection.
3. Keep geometry-specific parsing local to the projection, but reuse shared token/option rules.
4. Do not connect it to `MapEditorTab` until tests establish source ranges and object identity behavior.

Verification:

- New or existing core/map projection test target.
- Round-trip checks that projection reads without rewriting source.

Stop condition:

- Stop if projection design starts mutating source or adding UI dependencies. It must stay read-only.

### M7 - Map Read-Only Projection Consumers

Goal: replace Map read-only object discovery/reference lookups with `Th2GeometryProjection`.

Status: closed for production DOM-backed read-only paths. Remaining parsed-line references in Map are compatibility
fallbacks for contexts without DOM callbacks, focused compatibility tests, or source-rewrite planners that move to M8.

Candidate files:

- `src/app/text_editor/map_editor/MapEditorSceneRefreshController.cpp`
- `src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.cpp`
- `src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp`
- `src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp`
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp`

Steps:

1. Done: add a `MapEditorTab` projection/logical-source access context parallel to parsed-line compatibility callbacks.
2. Done: migrate selected object details, area-reference lookup, object discovery, selection navigation, scene refresh,
   Smart Area preview, background auto-adjust bounds, and context-menu metadata to DOM-backed logical/projection paths.
3. Done: keep `MapEditorTab::parsedLinesForCurrentDocument()` only as a compatibility adapter for contexts without DOM
   callbacks and for later M8 rewrite planner work.
4. Done: preserve current selection, hover, inspector, and partial-refresh behavior through focused map tests.
5. Deferred to M8/M9: remove compatibility APIs only after rewrite planners and compatibility tests have replacement
   coverage.

Verification:

- Map editor object/details tests.
- Map scene refresh or partial-refresh regression tests.
- Manual smoke: open representative `.th2`, select point/line/area/background, verify inspector and canvas.

Stop condition:

- Do not migrate source rewrites, move planners, line split, or background metadata writes in this slice.

### M8 - Transaction And Rewrite Planner Migration

Goal: make source-changing workflows consume DOM/projection source ranges while preserving undo/redo and formatting.

Candidate files:

- `src/core/TherionDocumentEditor.cpp`
- `src/app/text_editor/TextEditorSourceTransactionController.*`
- `src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.cpp`
- `src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp`
- `src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp`
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp`

Steps:

1. Audit each rewrite planner and split it into read-only planning and source mutation if needed.
2. For one planner at a time, replace local source-range inference with DOM/projection source ranges.
3. Keep explicit before/after source text input for planning until the transaction service owns revision checks.
4. Route the final mutation through the existing transaction controller/helper.
5. Add undo/redo and round-trip tests for each migrated workflow.

Priority order:

1. Non-map text-editor rewrites with low geometry risk.
2. Blocks move/delete/data-dialog rewrites.
3. Map object delete/move quick-field rewrites.
4. Line split and area-border reference rewrites.
5. Background metadata writes.

Verification:

- Existing planner tests plus new undo/redo or source transaction tests.
- CRLF and comment-preservation regressions for changed planners.

Stop condition:

- Stop if a planner cannot preserve source formatting from existing tests. Add projection/source-range support first.

### M9 - Legacy Removal

Goal: remove or quarantine duplicate parser paths after migrated consumers and tests are in place.

Status: complete for the `2026.7.2` DOM migration closure. `scripts/check_structure_constraints.py` now rejects new direct
`TherionDocumentParser::parseLine`, `parseTokenLines`, or `tokenizeLine` calls outside the documented exception list.

Final allowed legacy list:

- Parser implementation in `src/core/TherionDocumentParser.cpp`.
- Logical command construction in `src/core/TherionSourceLogicalDocument.cpp`.
- Validator synthetic single-line parsing in `src/core/TherionSourceValidator.cpp`.
- Command-option value snippet tokenization in `src/core/TherionCommandSyntax.cpp`.
- Source-editor single-line rewrite helper parsing in `src/core/TherionDocumentEditor.cpp`.
- Map pending-insert synthetic snippets in `MapEditorObjectDetailsPanelController.cpp` and
  `MapEditorTabSourceEditWorkflow.cpp`.

Steps:

1. Run the inventory command from M1.
2. Remove one production legacy caller at a time.
3. If a direct parser call remains, add a short comment only when it is intentionally allowed as `core DOM construction`,
   `synthetic snippet`, `non-Therion parsing`, or compatibility test coverage.
4. Add a structure constraint or test if a forbidden legacy path is likely to reappear.
5. Update this plan with the final allowed legacy list.

Verification:

- Affected test targets.
- `python3 scripts/check_structure_constraints.py`
- `git diff --check`

Stop condition:

- Do not remove public compatibility APIs while tests or production code still need them.

## Recommended 2026.7.2 Commit Order

1. Finish M0 and commit the current Blocks parsed-line helper slice.
2. M1 inventory and guardrails.
3. M2 shared snapshot access context.
4. M3 Raw cursor/help completion closure.
5. M4 Blocks read-only closure.
6. M5 Project/Structure/Search/Validation cache closure.
7. M6 TH2 projection contract with tests.
8. M7 Map read-only projection consumers.
9. M8 transaction/rewrite planner migration, one workflow per commit.
10. M9 legacy removal gates and final allowed legacy list.

## Verification Gates

Every migrated slice:

- Nearest focused automated tests.
- `python3 scripts/check_structure_constraints.py`.
- `git diff --check`.

Before declaring DOM migration complete:

- Full available local test pass for core, text editor, map editor, validation, project structure, and 3D-unrelated targets
  affected by shared source changes.
- Manual smoke pass with representative `.th`, `thconfig`, and `.th2` projects:
  - Raw editing, completion, help, validation tooltips.
  - Blocks cards/details on nested blocks, data bodies, continued commands, comments.
  - Map scene load, selection, inspector edits, object create/delete/move, line split, area references, backgrounds.
  - Structure tree and project validation on a multi-file project.
  - Compiler diagnostics link navigation.
- Release notes and user manual updates only if user-visible behavior changes. Pure architecture migration needs `WORKLOG.md`
  and this plan, but not end-user documentation.
