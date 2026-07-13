# Project Source Snapshot And Validation Optimization Plan

Date: 2026-06-30

Reviewed: 2026-07-13 against `plans/REVIEW_CODEX.md` and current implementation.

Status: initial project source snapshot/cache ownership slices are implemented. Keep this plan as a follow-up guide for
live validation performance, cache widening, and incremental project-index diagnostics.

Scope: build on the shared source snapshot and project scan cache architecture to reduce duplicate Structure/Validation
work, cache unchanged validation results, and prepare later incremental project-index diagnostics.

Relationship to the review: this plan retains snapshot/cache and live-validation optimization. Superseded
Structure/Outputs publication and asynchronous project watcher inventory are owned by
`plans/PROJECT_ASYNC_COORDINATION_PLAN.md` and are prerequisites before widening automatic project work.

## Current Findings

- Structure uses `ProjectStructureScanner`, which now collects `ProjectSourceSnapshot` inputs and passes them to `ProjectStructureIndex` through `ProjectStructureIndexSourceSet`.
- Validation uses `ProjectValidationScanner`, which now collects `ProjectSourceSnapshot` inputs once and uses them for local validation and project-index diagnostics.
- `ProjectStructureIndex::scanProjectIndex(ProjectStructureIndexSourceSet)` can build an index without rediscovering or rereading project files.
- Legacy `ProjectStructureIndex::scanProjectIndex(projectRoot, ...)` entry points still perform their own recursive `QDirIterator` discovery for callers that do not yet have snapshots.
- `ProjectScanCacheService` owns retained project source and project-index snapshot caches shared by Structure and
  Validation at the application workflow boundary.
- Project validation retains source/logical projections and per-file validation findings for repeated unchanged scans.
- `ProjectStructureIndex` uses `TherionSourceSnapshotCache` / `TherionSourceLogicalDocument` internally for parsing
  passes.
- `TextEditorTab::validateDocument()` has a document-revision keyed cache for the currently opened document, but project validation does not reuse that cache.
- Alternating automatic triggers can still stress cache windows and Validation UI updates; fresh diagnostics should guide
  any next optimization slice.
- Automatic project validation is disabled by default; manual validation is preferred until live diagnostics become incremental, cancellable, and cheap for nested projects.

## Goals

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

## Relationship To Shared Source Model

This plan is a follow-up performance and cache plan over the current shared source model.

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

Status: resolved for discovery/read duplication.

Validation now collects project source snapshots once, uses them for local validation, and builds project-index diagnostics through `ProjectStructureIndexSourceSet`. Remaining work is cache reuse of the local source/logical projections and project-index snapshot across repeated requests.

### Duplication B - Structure And Validation Independent Scans

Status: resolved for retained source and project-index snapshot ownership through `ProjectScanCacheService`.

Follow-up target: widen or tune retained cache windows only when logs show unchanged project source or project-index
projections are still being rebuilt.

### Duplication C - Repeated Per-File Validation

Status: repeated validation scans reuse per-file findings when source snapshot keys, catalog keys, and known project file
sets are unchanged.

Follow-up target: make live validation cheaper for nested projects before restoring automatic full-project validation as
the recommended/default mode.

## Core Types

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

The implementation supports persistent reuse across validation requests and exposes immutable logical-document handles
where consumers need stable projection lifetime.

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

Slice 1A - Project Source Request Key - Done

- Added a value type for project source collection inputs:
  - normalized project root
  - preferred config path
  - normalized in-memory contents by path
  - file filters / maximum validatable size policy where relevant
- Kept trigger/generation metadata outside the key.
- Added tests that equivalent path forms and in-memory maps produce stable keys.

Slice 1B - Project Source Snapshot Collector - Done

- Extracted file discovery and text collection into reusable `ProjectSourceSnapshot` collection.
- It supports:
  - disk files under project root
  - open in-memory overrides
  - files open in the editor but not discovered on disk yet, where existing Validation behavior supports them
  - size limit behavior compatible with current Validation
- Kept this collector independent from Structure and Validation UI.

Slice 1C - DOM Projection Access For Project Sources - Done

- `ProjectSourceProjectionCache` builds/reuses `TherionSourceDocument` and `TherionSourceLogicalDocument` projections.
- Validation owns a persistent shared cache instance rather than reconstructing projections for each file/request.
- Cache stats are observable for tests/logging.
- Existing tests cover:
  - unchanged source snapshot reuses projection within a run
  - changed in-memory text changes the key
  - catalog key changes only catalog-aware logical projection reuse

Verification:

- `TherionCoreQTests`
- new focused project source snapshot/cache tests
- `python3 scripts/check_structure_constraints.py`

## Phase 2 - Structure Uses Project Source Snapshots

Goal: route Structure/project index input through DOM-compatible project source snapshots before adding cross-consumer cache reuse.

Slice 2A - ProjectStructureIndex Snapshot Input - Done

- Added `ProjectStructureIndexSourceSet` and a `ProjectStructureIndex::scanProjectIndex()` overload that consumes already collected source text.
- Internally it still uses existing parsing routines, but it does not rediscover/read files when source-set text is provided.
- Preserved root config resolution behavior.
- Preserved in-memory document overrides.

Slice 2B - ProjectStructureScanner Snapshot Collection - Done

- Updated `ProjectStructureScanner` to collect `ProjectSourceSnapshot` and pass `ProjectStructureIndexSourceSet` to the index path.
- Preserved debounce and queued replacement behavior. `PROJECT_ASYNC_COORDINATION_PLAN.md` A1-A3 subsequently added
  request-acceptance serials, stale-result suppression, presentation guards, and deterministic supersession coverage.
- Diagnostics timing remains pending:
  - source collection ms
  - projection/index ms
  - source count

Slice 2C - Structure Regression Coverage - Done

- Existing Structure scanner tests should pass unchanged.
- Existing in-memory Structure scanner coverage verifies open in-memory text changes Structure output without saving the disk file.
- Added a direct `ProjectStructureIndexSourceSet` test proving provided snapshot text is used instead of stale disk text.

Verification:

- `ProjectStructureScannerTest`
- `ProjectStructureIndexTest`
- `python3 scripts/check_structure_constraints.py`

## Phase 3 - Validation Uses The Same Source Snapshots

Goal: make project validation derive local diagnostics and project-index diagnostics from the same source snapshot set.

Slice 3A - ProjectValidationScanner Snapshot Input - Done

- Updated validation to collect `ProjectSourceSnapshot`.
- Uses the snapshot set for per-file local validation.
- Kept existing validatable file filters and file-size behavior.
- Kept open in-memory document overrides identical to current behavior.

Slice 3B - Project Index From Validation Snapshot - Done

- Replaced Validation's internal unconditional `ProjectStructureIndex::scanProjectIndex(projectRootPath, inMemoryContents, ...)` call with the snapshot-input path.
- Kept project-index/cross-file diagnostics stable.
- Added regression coverage showing project-index diagnostics use unsaved in-memory source text.

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

Slice 4A - Bounded Project Source Snapshot Service - Done

- `ProjectScanCacheService` owns a bounded recent window of project source snapshots.
- `MainWindow` composes/shares the service while supplying root, preferred config, and in-memory contents.
- Request keys cover project/config/content/file-set identity and cache behavior has focused tests.

Slice 4B - Shared Project Index Snapshot - Done

- The service caches bounded `ProjectIndexSnapshot` results by source request key.
- Structure and Validation reuse the same index snapshot when source/config identity matches.
- Scanner results expose source/index cache hits for tests and diagnostics.

Slice 4C - Route Structure And Validation Through The Service - Done

- Structure and Validation request source/index projections through the shared service.
- Existing tests cover reuse across scanner instances/consumers with identical inputs.
- Further tuning requires timing/cache evidence rather than another ownership layer.

Verification:

- Existing scanner/controller tests.
- Existing cache-hit tests proving consumers with identical inputs reuse project source/index snapshots.

## Phase 5 - Per-File Local Validation Cache

Goal: avoid revalidating unchanged files across repeated project validation runs after source snapshot keys exist.

Slice 5A - Diagnostics-Only Cache Entry - Done

- Project validation cache entries store:
  - per-file local diagnostics from `TherionSourceValidator`
  - source key
  - catalog key
  - validation algorithm version
- Source/logical documents remain owned by `ProjectSourceProjectionCache` separately.

Slice 5B - Reuse Unchanged Local Diagnostics - Done

- Project validation reuses local diagnostics when the key matches and revalidates only:
  - changed files
  - new files
  - deleted-file removals
  - files affected by catalog/source-type changes
- Keep cross-file/project-index diagnostics separate and recomputed conservatively.
- Diagnostic logging reports cache hits/misses and timings.

```text
project-validation-cache files=44 local_hits=43 local_misses=1 local_validate_ms=3
```

Slice 5C - Open In-Memory Documents - Done

- In-memory editor text overrides disk text before computing the cache key.
- Existing tests cover unchanged reuse, in-memory invalidation, and catalog/source identity changes.
- Preserve tests for:
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

- Completed: `ProjectValidationScanner` consumes a `ProjectStructureIndexSourceSet` and no longer owns private recursive
  file discovery.
- Validation-only local text maps that duplicate project source snapshots.
- `ProjectStructureIndex::scanProjectIndex()` entry points that force disk discovery when callers already have snapshots.
- Per-run parsed caches that duplicate project-level source projection cache, once persistent cache coverage exists.

Do not remove until:

- Structure and Validation both consume project source snapshots.
- Project validation cache correctness tests exist.
- Reference and namespace regression coverage passes.
- Manual large-project diagnostics show improved repeated-run timings.

## Recommended Slice Queue

1. Complete `PROJECT_ASYNC_COORDINATION_PLAN.md` W1-W4 so recursive watcher discovery leaves the GUI thread.
2. Slice 3C: add an explicit Structure/Validation compatibility test over equivalent source snapshot identity.
3. Slice 6A: define stable validation finding identity for cheaper UI updates.
4. Slice 6B: short-circuit no-change Validation UI publication.
5. Slice 7A only after measurements show project-index rebuild is the next bottleneck.

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
