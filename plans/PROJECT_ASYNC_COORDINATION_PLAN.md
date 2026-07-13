# Project Async Coordination Plan

Date: 2026-07-13

Review findings: P1-3, P1-5.

Status: active; A1-A2 complete, A3 is next.

Scope: make Structure/Outputs result publication monotonic and move recursive project-watch inventory discovery off the
GUI thread. This plan does not redesign project source snapshots, validation caches, or platform watcher policy.

## Boundaries

- `ProjectStructureScanner` and `ProjectOutputsScanner` own request execution and latest-result publication.
- `ProjectValidationController` is the local reference for request serial/supersession semantics, not a base class to
  reuse mechanically.
- A project-watch inventory collector owns pure filesystem discovery and signatures.
- A project-watch workflow service owns background request/generation coordination.
- `QFileSystemWatcher` remains on the GUI thread; only inventory discovery moves to a worker.
- `MainWindow` supplies project identity, applies a completed inventory, and triggers existing invalidation workflows.

## Non-Goals

- Do not make `QFileSystemWatcher` thread-affine to a worker.
- Do not add cooperative traversal cancellation in the first scanner slice.
- Do not merge Structure, Outputs, Validation, and watcher requests into one generic scanner.
- Do not change project-index/cache keys or Structure/Outputs result content.
- Do not choose periodic reconciliation or a platform-specific watcher strategy without measurements.

## A1 — Define Latest-Request Serial Semantics — Complete

Allowed scope:

- `src/app/ProjectStructureScanner.h/.cpp`
- `src/app/ProjectOutputsScanner.h/.cpp`
- focused scanner tests

Steps:

1. Increment a monotonically increasing request serial when `requestScan()` accepts a request, not when the worker
   starts.
2. Copy that serial into the worker request and result.
3. On worker completion, publish only when the result serial equals the latest accepted serial.
4. Preserve the existing queued replacement: suppress A, then start the latest pending B.
5. Keep generation naming consistent across both scanners; do not retain two integers with ambiguous meaning.

Test seam: prefer an injected narrow scan function/executor or a pure publication gate so A/B overlap is deterministic.
Do not use fixed sleeps to hope A is still running.

Acceptance:

- A completes after B was accepted: A is never emitted;
- only the latest pending request runs after A;
- a lone request still emits exactly once;
- error results obey the same supersession rule.

Stop condition: if deterministic overlap requires a general task scheduler abstraction, extract only the scan callable
needed by these tests and stop before creating a repository-wide executor framework.

Outcome (2026-07-13): both scanners assign `requestSerial` when a request is accepted, carry it through the worker
result, and suppress completion unless it still matches the latest accepted serial. The existing queued replacement
starts only the latest pending request. Narrow injected scan callables let the focused tests hold request A at a
deterministic barrier, replace pending B with C, and verify stale success/error publication without fixed-delay sleeps or
a general executor abstraction. Consumer-side defense remains A2 scope.

## A2 — Guard Structure And Outputs Consumers — Complete

Allowed scope:

- `src/app/MainWindowStructureBrowser.cpp`
- `src/app/MainWindowOutputsSidebar.cpp`
- focused controller/UI-state tests

Steps:

1. Keep a defensive latest-request/result identity check at the presentation boundary.
2. Verify both same-root content replacement and project-root replacement.
3. Ensure stale errors and busy/empty states cannot replace the current surface.

The scanner is the primary suppression owner; the UI check is defense in depth, not a second independent timeline.

Outcome (2026-07-13): both presentation handlers reject results whose request serial is no longer current before they
clear models, show errors, append console output, or apply Structure state. They also require the normalized result root
to match the current project, including the empty-project state. Focused identity tests cover same-root replacement and
project-root replacement, stale errors remain suppressed, and an empty Outputs request now reports the intended
open-project state instead of resolving the empty path to the process working directory.

## A3 — Scanner Verification And Touched-Test Migration

- Migrate the touched hand-rolled `ProjectStructureScannerTest` to the appropriate app/service QTest runner if that
  runner boundary exists; otherwise retain isolation and document why.
- Extend `ProjectOutputsScannerTest` with deterministic A/B supersession.
- Run both tests repeatedly without `sleep`-based ordering.
- Run Structure and Outputs navigation/opening smoke paths.

Exit gate for A-series: no superseded Structure or Outputs result is observable by consumers.

## W1 — Extract Pure Project Watch Inventory

Introduce focused value types, for example:

- `ProjectFileWatchInventoryRequest`
- `ProjectFileWatchInventory`
- `ProjectFileWatchInventoryCollector`

The inventory contains normalized project root, directories, Therion source files, per-path signatures, skipped paths,
and discovery errors. Preserve current skip rules for VCS/build directories, symlinks, and `.th`/`.th2`/config files.

Allowed scope:

- new focused files under `src/app/`
- `CMakeLists.txt`
- app/service QTests using `QTemporaryDir`

Acceptance:

- path ordering and signatures are deterministic;
- symlink loops and skipped directories are excluded;
- files outside the root cannot enter the inventory;
- collector has no widgets, `MainWindow`, or `QFileSystemWatcher` dependency.

## W2 — Add Generation-Keyed Inventory Service

Add a QObject workflow service that runs W1 through `QtConcurrent` or an equivalent existing Qt worker boundary.

Requirements:

- latest-request serial is assigned at request acceptance;
- stale inventories are suppressed;
- teardown is QObject-lifetime safe;
- the service emits inventory data only and never mutates a `QFileSystemWatcher`;
- diagnostic timing includes root, directory/file counts, discovery time, and superseded state.

Tests: same-root replacement, changed-root replacement, teardown during work, discovery error.

## W3 — Apply Watcher Deltas On The GUI Thread

Update `MainWindowProjectFileWatcher.cpp` and `MainWindow.h` to request inventory asynchronously and apply it only when
its project root/serial is current.

Apply behavior:

- compute add/remove deltas against current watcher paths instead of clearing first when practical;
- call `QFileSystemWatcher::addPaths/removePaths` only on the GUI thread;
- retain signatures only for successfully current paths;
- record paths returned as failed by `addPaths()` and surface bounded diagnostic information;
- preserve existing Structure/Map/Outputs/Validation invalidation after a real mutation.

Stop condition: watcher-limit policy changes are out of scope. If platform limits remain a problem, record measurements
and plan a later hybrid policy.

## W4 — Large-Tree And Mutation Verification

- generated deep/wide temporary tree with skipped directories and symlink cases;
- UI heartbeat/event-loop responsiveness while inventory builds;
- rapid open project A → project B;
- directory addition/removal causes one latest inventory application;
- changed Therion file still invalidates shared scan cache and requests Structure/Outputs/Validation refresh;
- macOS, Windows, and Linux focused CI.

## Relationship To Other Plans

- `PROJECT_SCAN_VALIDATION_OPTIMIZATION_PLAN.md` retains source snapshot/cache/live-validation work. A-series is a
  prerequisite for further automatic scan widening.
- Watch inventory may reuse deterministic path policies from `ProjectFileDiscovery`, but shall not force watcher data
  into validation source snapshots.
- SQL work uses a separate worker because SQLite connection ownership and interruption have different constraints.

## Exit Gate

- Structure/Outputs publish only latest requests.
- Recursive project watch traversal and signature construction do not run on the GUI thread.
- Watcher mutation remains GUI-thread-only and failed watch paths are diagnosable.
- Existing project invalidation behavior and open unsaved document handling remain unchanged.
