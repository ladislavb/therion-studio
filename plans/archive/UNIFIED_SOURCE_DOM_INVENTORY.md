# Unified Source DOM Inventory

Archived snapshot from the completed Unified Source DOM migration. The active migration plan is archived beside this file
as `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md`; current parser exception rules are enforced by
`scripts/check_structure_constraints.py`.

This file records the M1 legacy parser/line-splitting inventory as it existed during the migration. It is retained for
audit/history only and should not be used as the current call-site list.

The snapshot below was gathered from:

```text
rg -n "TherionDocumentParser::parseLine|parseTokenLines|tokenizeLine|splitTextLines|detectedLineEnding" src tests
```

## Allowed To Remain

These call sites are intentionally acceptable for now because they are core DOM construction, synthetic snippets,
compatibility tests, or non-Therion parsing:

| File | Why it may remain |
| --- | --- |
| `src/core/TherionDocumentParser.cpp` | Core parser implementation. |
| `src/core/TherionCommandSyntax.cpp` | Tokenization used by shared command syntax helpers. |
| `src/core/TherionSourceLogicalDocument.cpp` | Logical-document construction from parsed physical source. |
| `src/core/TherionSourceValidator.cpp` | Validation still consumes parser output for synthetic helper frames. |
| `src/core/TherionDocumentEditor.cpp` | Legacy rewrite planning and source mutation plumbing. |
| `src/core/TherionBackgroundMetadata.cpp` | Background metadata rewrite still uses low-level source line handling. |
| `src/app/text_editor/CommandOptionsDialog.cpp` | User-entered command snippets, not full-document DOM projection. |
| `src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp` | Raw token input from the editor widget. |
| `src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.cpp` | Synthetic block snippet parsing. |
| `tests/TherionDocumentParserTest.cpp` | Parser behavior coverage. |
| `tests/MapEditorObjectDetailsLogicTest.cpp` | Synthetic command snippets for unit coverage. |
| `tests/MapEditorInspectorDataTest.cpp` | Parser coverage for inspector behavior. |
| `tests/MapGeometryFeatureParsingTest.cpp` | TH2 compatibility and geometry guardrail coverage. |
| `tests/ProjectStructureIndexTest.cpp` | Structure/index regression coverage. |
| `tests/BlockEditorCanvasRebuildControllerTest.cpp` | Regression coverage for helper migration. |

## Migration Candidates

These are the current production hotspots that should be migrated to shared DOM/projection helpers in small slices.

| File | Current use | Planned replacement |
| --- | --- | --- |
| `src/editor/TherionSyntaxHighlighter.cpp` | Full-line parsing for highlighting. | Shared source snapshot / logical line projection. |
| `src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp` | Block deletion planning uses local parsed lines. | Shared Blocks logical helper. |
| `src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp` | Line-body parsing for Blocks. | Logical command helper / parsed-line reuse. |
| `src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp` | Outline scanning still reparses rows. | Logical command projection. |
| `src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp` | Tokenization of option values. | Shared token helper or DOM option ranges. |
| `src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp` | Local line scans for inspector state. | TH2 geometry projection. |
| `src/app/text_editor/map_editor/MapEditorObjectDetailsEditController.cpp` | Cursor/text-line parsing in rewrite planning. | Transaction-aware projection/range helper. |
| `src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.cpp` | Local parsed-line scans during delete planning. | TH2 geometry projection + transaction helper. |
| `src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp` | Token-line parsing for movement planning. | TH2 geometry projection + transaction helper. |
| `src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp` | Token-line parsing and line-range scans. | TH2 geometry projection + transaction helper. |
| `src/app/text_editor/map_editor/MapEditorSceneRefreshController.cpp` | `parseTokenLines()` compatibility projection. | `Th2GeometryProjection`. |
| `src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.cpp` | Reference lookup from token-line parsing. | Shared geometry projection. |
| `src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp` | Area-reference lookup from token-line parsing. | Shared geometry projection. |
| `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp` | Background metadata and row scans. | Background-aware DOM projection. |
| `src/app/text_editor/map_editor/MapEditorTabSourceEditWorkflow.cpp` | Cached token-line compatibility projection. | `Th2GeometryProjection` / source snapshot cache. |
| `src/app/text_editor/map_editor/MapEditorTabSelectionInspectorWorkflow.cpp` | Line parsing for selection inspection. | Shared geometry projection. |
| `src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp` | Legacy fallback parse for scan lines. | Keep only until read-only helper coverage is complete. |
| `src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp` | Legacy fallback parse for details rows. | Keep only until read-only helper coverage is complete. |
| `src/app/TextEditorModeController.cpp` | Mode detection still parses document lines directly. | Shared source snapshot helper. |
| `src/app/ProjectSearchScanner.cpp` | Plain text line splitting for search traversal. | Keep if search is intentionally text-based; otherwise shared source text helper. |

## Compatibility Boundaries

These are the current intentional compatibility boundaries that should not be removed until the projection contract is
fully established:

| Boundary | Reason |
| --- | --- |
| `MapEditorTab::parsedLinesForCurrentDocument()` | TH2 compatibility bridge until geometry projection exists. |
| `TherionDocumentEditor.cpp` rewrite planners | Source mutation ownership is still being consolidated. |
| `MapGeometryFeatureParsingTest.cpp` | Legacy token-line compatibility guardrail for TH2 behavior. |
| `TextEditorCaretInteractionTest.cpp` legacy parser uses | Synthetic regression coverage for source behavior. |
| `TherionSourceValidator` parser frames | Validation still needs small synthetic parser frames for explicit diagnostics. |

## Guardrails For The Next Slices

- Do not add new UI-side reparsing when a `TherionSourceDocument` or `TherionSourceLogicalDocument` helper can be used.
- Do not remove `parseTokenLines()` compatibility users until a TH2 geometry projection has equivalent test coverage.
- Keep direct `parseLine()` calls for synthetic snippets and tests only when they do not represent a full document
  projection.
- Keep `splitTextLines()` and `detectedLineEnding()` in the core text/round-trip layer, not in widgets.
- Prefer adding a focused helper over cloning parser logic into another consumer.

## Suggested Immediate Next Slice

The next practical migration slice is one read-only Raw consumer that still maps cursor positions to tokens without
using `TherionSourceLogicalDocument::tokenAtOffset()`. After that, continue through the Blocks read-only consumers and
then the Map read-only projection boundary.
