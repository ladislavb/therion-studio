# Project Source Snapshot And Validation Optimization Plan

Date: 2026-06-30

Scope: continue the Unified Source DOM migration into project-level source snapshots first, then use those snapshots to remove duplicate Structure/Validation scans, cache per-file validation, and prepare later incremental project-index diagnostics.

This plan replaces an optimization-first approach with a DOM-first sequence. The performance goal remains important, but the first implementation slices should establish shared project source/projection boundaries so later scan and validation optimizations are not built around legacy ad hoc file discovery and reparsing.

## Current Findings

- Structure uses `ProjectStructureScanner`, which runs `ProjectStructureIndex::scanProjectIndex()` in a worker thread.
- Validation uses `ProjectValidationScanner`, which performs its own recursive validatable-file discovery, file reads/in-memory text capture, and per-file `TherionSourceValidator` pass.
- During the same validation run, `ProjectValidationScanner` then calls `ProjectStructureIndex::scanProjectIndex()` again to obtain project-index diagnostics and source graph information.
- `ProjectStructureIndex::scanProjectIndex()` performs its own recursive `QDirIterator` file discovery and owns a per-run `ParsedFileCache`.
- `ProjectValidationScanner` owns a per-run `TherionSourceSnapshotCache`; it does not persist across scans.
- `ProjectStructureIndex` already uses `TherionSourceSnapshotCache` / `TherionSourceLogicalDocument` internally for several parsing passes, but the cache lifetime is one project-index scan.
- `TextEditorTab::validateDocument()` has a document-revision keyed cache for the currently opened document, but project validation does not reuse that cache.
- Structure and Validation can therefore scan and parse overlapping files independently when they run close together.
- Automatic project validation is disabled by default; manual validation is preferred until live diagnostics become incremental, cancellable, and cheap for nested projects.

## DOM-First Goals

- Introduce a shared project source snapshot boundary before adding broad scanner caches.
- Represent project files and open in-memory overrides as stable source snapshot inputs that can feed `TherionSourceDocument` / `TherionSourceLogicalDocument`.
- Make Structure, Validation, Search, and later Map/project reference consumers derive from the same source snapshot model.
- Avoid duplicate project index scans between Structure and Validation as a consequence of shared project snapshots.
- Avoid revalidating unchanged files on repeated project validation requests after snapshot keys exist.
- Keep diagnostics correct for open unsaved documents by keying caches on actual in-memory text, not only file metadata.
- Keep Structure as an orientation/navigation projection and Validation as the central problem-reporting surface.
- Keep project scan work asynchronous, debounced, generation-keyed, and safe to supersede.

## Non-Goals

- Do not make automatic full-project validation the default in the first slice.
- Do not implement a fully incremental dependency graph before shared source snapshots and per-file projection keys exist.
- Do not move project scanning into UI widgets.
- Do not merge Structure and Validation UI models.
- Do not remove `ProjectStructureIndex::scanProjectIndex()` callers until replacement coverage exists.
- Do not cache diagnostics without a content/catalog/source-type key.
- Do not solve Map/TH2 geometry projection in this plan; that remains covered by the Unified Source DOM and Map partial refresh plans.

## Relationship To Unified Source DOM

This plan is the detailed execution track for the Unified Source DOM plan's Structure/project-index/diagnostics phase.

The intended dependency direction is:

```text
Project source collection
  -> ProjectSourceSnapshot entries
  -> TherionSourceDocument / TherionSourceLogicalDocument
  -> project projections
  -> Structure / Validation / Search / future Map consumers
```

The short-term implementation may still call existing `ProjectStructureIndex` internals, but new public seams should accept source snapshots or projection inputs rather than forcing each consumer to perform its own directory scan and text read.

## Current Duplication To Remove

### Duplication A - Validation Internal Double Work

Validation currently:

1. discovers validatable files and reads text for local validation;
2. validates every file;
3. calls `ProjectStructureIndex::scanProjectIndex()`, which discovers/parses project files again for project-index diagnostics.

DOM-first target: validation should receive a `ProjectSourceSnapshotSet` and project index projection derived from the same source snapshot inputs.

### Duplication B - Structure And Validation Independent Scans

Structure and Validation each own their own scanner, debounce timer, worker, and project-index construction path.

DOM-first target: Structure and Validation request projections from the same project source snapshot service when project root, preferred config, in-memory contents, catalog key, and disk state match.

### Duplication C - Repeated Per-File Validation

Repeated validation requests validate all files even when only one open file changed.

DOM-first target: cache local per-file logical source projections and validation findings by source snapshot key and catalog key.

## Core Types To Introduce

The exact names may change during implementation, but the responsibilities should stay narrow.

### ProjectSourceSnapshot

Represents one project source file, including in-memory overrides.

```cpp
struct ProjectSourceSnapshot
{
    QString normalizedPath;
    TherionSourceDocumentType sourceType = TherionSourceDocumentType::Unknown;
    QString contents;
    QByteArray contentHash;
    qint64 diskSize = -1;
    QDateTime diskLastModifiedUtc;
    bool fromInMemoryDocument = false;
    int syntheticRevision = 0;
};
```

Rules:

- `contents` is the exact text that project consumers should use.
- Open in-memory editor text overrides disk text before `contentHash` is computed.
- Disk metadata is useful for cheap invalidation, but content hash/revision is the correctness key.
- Synthetic revisions are project-scan-local unless later replaced with a persistent project source cache revision.

### ProjectSourceSnapshotSet

Represents one normalized project source collection request/result.

```cpp
struct ProjectSourceSnapshotSet
{
    QString projectRootPath;
    QString preferredConfigPath;
    QVector<ProjectSourceSnapshot> sources;
    QHash<QString, int> sourceIndexByPath;
    QString sourceSetKey;
};
```

Rules:

- Path normalization happens once.
- The source set key includes project root, preferred config path, source path list, source content keys, and in-memory overrides.
- This type should not contain Structure or Validation UI state.

### ProjectSourceProjectionCache

Owns DOM/logical projections by source key and catalog key.

Initial implementation may be single-run or single-entry, but its API should be compatible with later persistent cache ownership.

Responsibilities:

- produce `TherionSourceDocument` from `ProjectSourceSnapshot`;
- produce catalog-free `TherionSourceLogicalDocument`;
- produce catalog-aware `TherionSourceLogicalDocument`;
- report cache hit/miss counts for diagnostics.

Non-responsibilities:

- no UI model building;
- no file dialogs or widget access;
- no source mutation;
- no direct Validation tree ownership.

## Cache Key Model

Project source set key:

- normalized project root
- preferred/root config path
- normalized source file path set
- each source's content hash/revision
- in-memory override marker
- file discovery/filter algorithm version

Source projection key:

- normalized path
- source type
- encoding metadata when available
- content hash or document revision
- projection algorithm version

Catalog-aware logical projection key:

- source projection key
- command catalog revision/key
- catalog-aware projection algorithm version

Per-file validation key:

- catalog-aware logical projection key
- validation algorithm version

Project index key:

- project source set key
- preferred/root config path
- project-index algorithm version

Do not reuse cache entries when:

- an open editor has newer in-memory text for the file;
- command catalog metadata changes;
- root config selection changes;
- the file enters or leaves the project source set;
- validation/project-index/source projection algorithm version changes.

## Phase 1 - DOM Project Source Snapshot Boundary

Goal: create a shared project source input model without changing Structure or Validation behavior yet.

Slice 1A - Project Source Request Key

- Add a value type for project source collection inputs:
  - normalized project root
  - preferred config path
  - normalized in-memory contents by path
  - file filters / maximum validatable size policy where relevant
- Keep trigger/generation metadata outside the key.
- Add tests that equivalent path forms and in-memory maps produce stable keys.

Slice 1B - Project Source Snapshot Collector

- Extract file discovery and text collection into a reusable collector.
- It should produce `ProjectSourceSnapshotSet`.
- It should support:
  - disk files under project root
  - open in-memory overrides
  - files open in the editor but not discovered on disk yet, where existing Validation behavior supports them
  - source type detection by filename
  - size limit behavior compatible with current Validation
- Keep this collector independent from Structure and Validation UI.

Slice 1C - DOM Projection Access For Project Sources

- Add a small `ProjectSourceProjectionCache` that can build `TherionSourceDocument` and `TherionSourceLogicalDocument` for a `ProjectSourceSnapshot`.
- Start with a per-run cache if persistent ownership would make the first slice too large.
- Make cache stats observable for tests/logging.
- Add tests:
  - unchanged source snapshot reuses projection within a run
  - changed in-memory text changes the key
  - catalog key changes only catalog-aware logical projection reuse

Verification:

- `TherionCoreQTests`
- new focused project source snapshot/cache tests
- `python3 scripts/check_structure_constraints.py`

## Phase 2 - Structure Uses Project Source Snapshots

Goal: route Structure/project index input through DOM-compatible project source snapshots before adding cross-consumer cache reuse.

Slice 2A - ProjectStructureIndex Snapshot Input

- Add an overload or adapter that lets `ProjectStructureIndex` consume `ProjectSourceSnapshotSet`.
- Internally it may still use existing parsing routines, but should not rediscover/read files when snapshots are provided.
- Preserve root config resolution behavior.
- Preserve in-memory document overrides.

Slice 2B - ProjectStructureScanner Snapshot Collection

- Update `ProjectStructureScanner` to collect `ProjectSourceSnapshotSet` and pass it to the index path.
- Preserve existing result shape and debounce/supersede behavior.
- Add diagnostics timing:
  - source collection ms
  - projection/index ms
  - source count

Slice 2C - Structure Regression Coverage

- Existing Structure scanner tests should pass unchanged.
- Add a test where an open in-memory file changes the Structure output without requiring the disk file to be saved.
- Add a test that Structure does not perform an extra discovery/read pass when given a snapshot set, if practical with instrumentation.

Verification:

- `ProjectStructureScannerTest`
- `ProjectStructureIndexTest`
- `python3 scripts/check_structure_constraints.py`

## Phase 3 - Validation Uses The Same Source Snapshots

Goal: make project validation derive local diagnostics and project-index diagnostics from the same source snapshot set.

Slice 3A - ProjectValidationScanner Snapshot Input

- Update validation to collect `ProjectSourceSnapshotSet`.
- Use the snapshot set for per-file local validation.
- Keep existing validatable file filters and file-size behavior.
- Keep open in-memory document overrides identical to current behavior.

Slice 3B - Project Index From Validation Snapshot

- Replace Validation's internal unconditional `ProjectStructureIndex::scanProjectIndex(projectRootPath, inMemoryContents, ...)` call with the snapshot-input path.
- Keep project-index/cross-file diagnostics identical.
- Keep fallback full index scan only when snapshot input is unavailable or invalid.

Slice 3C - Validation/Structure Compatibility Test

- Add a fixture test that runs Structure and Validation on the same `ProjectSourceSnapshotSet` and confirms:
  - same project root/config interpretation
  - same in-memory override content
  - project-index diagnostics remain stable
- Keep result ordering stable where existing UI/tests depend on it.

Verification:

- `ProjectValidationScannerTest`
- `ProjectValidationControllerTest`
- `ProjectStructureScannerTest`
- `ProjectStructureIndexTest`
- `TherionSourceValidatorProjectionTest`

## Phase 4 - Shared Project Snapshot Service

Goal: remove duplicate Structure/Validation project source collection and project index construction when inputs match.

Slice 4A - Single-Entry Project Source Snapshot Service

- Add a focused service that owns the latest `ProjectSourceSnapshotSet`.
- Start with a single-entry "last project source snapshot" cache.
- Keep ownership outside `MainWindow`; `MainWindow` supplies project root, preferred config, and in-memory contents.
- Log invalidation reason:
  - project root changed
  - preferred config changed
  - in-memory content changed
  - disk metadata/content changed
  - file set changed
  - no cached snapshot

Slice 4B - Shared Project Index Snapshot

- Extend the service to cache the `ProjectIndexSnapshot` derived from a source snapshot key.
- Structure and Validation can reuse it when the source snapshot key and preferred config match.
- Do not make the cache unbounded.
- Keep worker/generation semantics explicit and testable.

Slice 4C - Route Structure And Validation Through The Service

- Structure requests the source/index projection from the shared service.
- Validation requests the same source/index projection before local validation.
- If both requests arrive close together, only one index projection should be needed for identical inputs.
- Add diagnostic logs showing project-index cache hit/miss.

Verification:

- Existing scanner/controller tests.
- New test proving two consumers with identical inputs reuse the same project index snapshot or at least avoid a second `scanProjectIndex()` call through instrumentation.

## Phase 5 - Per-File Local Validation Cache

Goal: avoid revalidating unchanged files across repeated project validation runs after source snapshot keys exist.

Slice 5A - Diagnostics-Only Cache Entry

- Add cache entries storing:
  - per-file local diagnostics from `TherionSourceValidator`
  - source key
  - catalog key
  - validation algorithm version
- Start diagnostics-only; cache source/logical documents through `ProjectSourceProjectionCache` separately.

Slice 5B - Reuse Unchanged Local Diagnostics

- During project validation, reuse local diagnostics when the key matches.
- Revalidate only:
  - changed files
  - new files
  - deleted-file removals
  - files affected by catalog/source-type changes
- Keep cross-file/project-index diagnostics separate and recomputed conservatively.
- Add diagnostic logging:

```text
project-validation-cache files=44 local_hits=43 local_misses=1 local_validate_ms=3
```

Slice 5C - Open In-Memory Documents

- Ensure in-memory editor text overrides disk text before computing the cache key.
- Add tests:
  - unchanged disk file reuses local diagnostics
  - changed in-memory text invalidates only that file
  - saving the same text does not duplicate findings
  - catalog key change invalidates local diagnostics

Verification:

- `ProjectValidationScannerTest`
- `ProjectValidationControllerTest`
- `TherionSourceValidatorProjectionTest`
- manual diagnostic check on a nested project with automatic validation enabled

## Phase 6 - Validation UI Incremental Update

Goal: avoid rebuilding the entire Validation tree when findings are unchanged or only one file changes.

Slice 6A - Stable Finding Identity

- Define finding identity:
  - normalized file path
  - diagnostic code
  - line/column
  - current text or source range key where needed
- Use it to preserve selection and compare result sets.

Slice 6B - No-Change UI Short-Circuit

- If a validation result has the same finding identity list as the current model, update status/timestamps only.
- Keep project diagnostics applied to open editors only when diagnostics for that file changed.

Slice 6C - Per-File Model Update

- Later, update only changed file nodes in the Validation tree.
- Keep full model rebuild fallback for error states, root changes, and large structural changes.

Verification:

- Tests around `handleProjectValidationFinished()` may need extraction into a view-model/service first.
- Existing validation navigation/fix tests must continue passing.

## Phase 7 - Incremental Project Index

Goal: reduce cross-file diagnostic recomputation after shared project snapshots and local validation cache are stable.

Slice 7A - Dependency Graph Inventory

- Explicitly model:
  - root config -> source graph
  - file -> included/source files
  - file -> defined surveys/maps/scraps/stations/object ids
  - file -> references to maps/scraps/stations/joins
  - namespace context dependencies
- Keep Therion qualified reference order exactly as `object@child.parent`.

Slice 7B - Per-File Project Index Fragments

- Split `ProjectStructureIndex` into per-file fragments plus a merge/resolve pass.
- Cache fragments by `ProjectSourceSnapshot` key.
- Re-merge fragments conservatively when any source graph/root config input changes.

Slice 7C - Affected Diagnostics

- Recompute diagnostics only for:
  - changed file fragments
  - files depending on changed definitions/references
  - source graph roots affected by include/source changes
- Keep full index rebuild fallback for root config changes, deleted files, namespace ambiguity, or unknown dependency state.

Verification:

- Project index tests for source graph, namespace references, duplicate ids, station references, join references, and map/scrap references.
- Add targeted tests for one-file changes affecting another file's diagnostic.

## Phase 8 - Legacy Removal Gates

Goal: remove duplicate discovery/read/parse paths only after the DOM-backed project snapshot path owns the behavior.

Removal candidates:

- `ProjectValidationScanner` private recursive file discovery.
- Validation-only local text maps that duplicate project source snapshots.
- `ProjectStructureIndex::scanProjectIndex()` entry points that force disk discovery when callers already have snapshots.
- Per-run parsed caches that duplicate project-level source projection cache, once persistent cache coverage exists.

Do not remove until:

- Structure and Validation both consume project source snapshots.
- Project validation cache correctness tests exist.
- Reference and namespace regression coverage passes.
- Manual large-project diagnostics show improved repeated-run timings.

## Recommended Slice Queue

1. Slice 1A: introduce normalized project source request key.
2. Slice 1B: extract project source snapshot collector.
3. Slice 1C: add project source DOM projection cache access.
4. Slice 2A: let `ProjectStructureIndex` consume project source snapshots.
5. Slice 2B: route `ProjectStructureScanner` through source snapshots.
6. Slice 3A: route `ProjectValidationScanner` local validation through source snapshots.
7. Slice 3B: route Validation project-index diagnostics through the snapshot-input index path.
8. Slice 4A: add single-entry project source snapshot service.
9. Slice 4B: add shared project index snapshot reuse for Structure/Validation.
10. Slice 5A: add diagnostics-only per-file validation cache.
11. Slice 5B: reuse unchanged local diagnostics and log hits/misses.
12. Slice 6A: define stable validation finding identity for cheaper UI updates.
13. Slice 7A: inventory dependency graph before incremental project-index implementation.

## Acceptance Criteria

- A shared project source snapshot model exists and includes disk files plus open in-memory overrides.
- Structure can build its project index from project source snapshots without rediscovering/re-reading project files.
- Validation can run local diagnostics and project-index diagnostics from the same source snapshot inputs.
- Structure and Validation can share a current `ProjectIndexSnapshot` for the same project root, preferred config, in-memory document contents, and disk state.
- Repeated validation of an unchanged project reuses local per-file diagnostics.
- Editing one open document invalidates local diagnostics for that document without forcing all unchanged files through `TherionSourceValidator` again.
- Project-index/cross-file diagnostics remain correct and may continue to use full rebuild until incremental dependency tracking is implemented.
- Diagnostic logs can distinguish source collection, projection cache, local validation, project index, cache hit/miss, and UI update costs.
- Automatic validation remains opt-in until repeated-run cost is low on nested projects.

## Verification Gates

- `ProjectStructureScannerTest`
- `ProjectValidationScannerTest`
- `ProjectValidationControllerTest`
- `ProjectStructureIndexTest`
- `TherionSourceValidatorProjectionTest`
- `TherionCoreQTests`
- `python3 scripts/check_structure_constraints.py`
- Manual or diagnostic-log comparison on a nested project:
  - first validation after project open
  - repeated validation without changes
  - validation after one open document edit
  - validation after root config/source graph change

## Risk Areas

- Reusing a stale project source snapshot when in-memory editor text differs from disk.
- Reusing local diagnostics after command catalog metadata changes.
- Incorrect root config keying when the compiler target config changes.
- Hiding cross-file diagnostics because only local file diagnostics were refreshed.
- Overcomplicating the first slice with full incremental dependency tracking.
- Moving project scan ownership into `MainWindow` instead of a focused service.
- Confusing Structure orientation data with Validation problem reporting data.
- Building caches around legacy scanner internals before project source snapshot boundaries exist.
