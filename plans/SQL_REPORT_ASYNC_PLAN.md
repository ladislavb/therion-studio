# SQL Report Responsiveness Plan

Date: 2026-07-13

Review findings: P1-2 and the report-specific part of P2-1.

Status: complete; S1-S5 complete.

Scope: move Therion SQL import and read-only report queries behind a worker-owned SQLite connection with request
supersession, real interruption, bounded execution, and safe tab teardown. Preset persistence and CSV file output move
out of the widget only after the worker lifecycle is stable.

## Required Ownership

- `TherionSqlReportTab` owns widgets, busy/error presentation, latest request identity, and immutable table DTOs.
- `TherionSqlReportWorker` owns `TherionSqlReportDatabase`, its native SQLite connection, import/query execution, and
  connection teardown.
- The SQLite connection is created, used, and destroyed on one dedicated worker thread.
- Queued signals cross the thread boundary only with value requests/results; never with native SQLite handles,
  prepared statements, model, or widget pointers.
- The existing read-only statement policy, expected-schema validation, row cap, presets, and CSV output remain stable.

## Non-Goals

- Do not move the widget or table model to a worker thread.
- Do not share a SQL connection between UI and worker threads.
- Do not add charts, filters, write queries, or report features.
- Do not treat a UI timer or an atomic flag alone as cancellation of a currently executing SQLite statement.
- Do not refactor all settings/dialog/file workflows in the application.

## S1 — Prove The Worker-Owned Database Contract — Complete

Allowed scope:

- new `src/app/reports/TherionSqlReportWorker.*`
- minimal value request/result types
- `TherionSqlReportDatabase.*` changes required for worker construction/teardown
- `CMakeLists.txt`
- focused app/service QTests

Steps:

1. Create the database inside the worker thread, not in the tab constructor and not before `moveToThread()`.
2. Add import and query requests carrying request ID, source identity, query text, row limit, and execution policy.
3. Emit value results with request ID, source identity, table/schema DTO, error code/message, and cancelled state.
4. Close and remove the named Qt SQL connection in the same worker thread after queries are destroyed.
5. Test thread identity and deterministic teardown without a widget.

Acceptance:

- every Qt SQL operation and connection lifecycle event occurs on the worker thread;
- no SQL connection warning appears during teardown;
- malformed import rolls back and returns an actionable value error;
- the worker has no QWidget or dialog dependency.

Stop condition: any design requiring a connection created on the GUI thread or returned to the GUI thread is rejected.

Outcome (2026-07-13): `TherionSqlReportWorker` now accepts value import/query requests and emits value results with
request/source identity, execution policy, schema/table DTOs, error code/message, and cancellation state. It constructs
and owns `TherionSqlReportDatabase` only in its affinity thread. The database records its construction thread, asserts
all SQL access against it, and closes/removes named connections after local queries are destroyed. App/service QTests
cover worker-thread lifecycle events, valid import/query, source mismatch, malformed transactional rollback, recovery,
and warning-free deterministic teardown. The tab is intentionally unchanged and remains S2 scope.

## S2 — Define Async Tab Load And Query Semantics — Complete

Read callers of `TherionSqlReportTab::loadFile()` before editing. Preserve the generic document-open contract by making
the synchronous return mean "request accepted/path valid" and present later import/schema failure through the tab's
normal error state, or introduce an explicit async open result at the application workflow boundary. Do not silently
reinterpret failure without updating callers/tests.

Steps:

1. Replace the tab-owned database with the worker/controller boundary.
2. Assign a monotonically increasing request ID when import/query is accepted.
3. Disable conflicting controls and show translatable busy/cancel state.
4. Apply a result only when request ID and current source identity still match.
5. Query B supersedes query A; reload/new file supersedes all queries for the previous database.
6. Closing the tab requests cancellation, disconnects publication, and tears the worker down without UI callbacks.

Tests:

- event-loop heartbeat during a generated large import;
- query A then B publishes only B;
- load file A then B cannot publish A's schema/table;
- import error leaves a coherent empty/previous state according to the declared policy;
- close tab during import/query is bounded and safe.

Outcome (2026-07-13): `MainWindow` now explicitly composes a `TherionSqlReportWorkerSession` and injects the narrow
session contract into `TherionSqlReportTab`. Synchronous `loadFile()` success means that a readable path was accepted;
import/schema failures arrive later through the tab error state. The tab clears previous file projections at accepted
load, disables conflicting controls while importing, and accepts import/query results only when both request ID and
source generation remain current. Newer query and load requests therefore suppress older table/schema publication.
Closing the tab disconnects publication and initiates non-blocking session teardown; active SQLite work may still drain
in the detached worker because actual statement interruption remains explicitly S3 scope. A focused QApplication QTest
covers large-import event-loop heartbeat, A/B load and query supersession, coherent import failure, and bounded tab close.

## S3 — Add Real SQLite Interruption And Deadline Policy

First perform a focused feasibility slice for the deployed QSQLITE driver. A valid implementation must reach SQLite's
progress/interruption mechanism through a supported, narrowly wrapped native handle and must be compatible with all
packaged platforms.

Requirements:

- cooperative cancel is observable inside a long-running statement;
- an execution deadline/progress budget protects against recursive or computationally expensive read-only queries;
- cancellation and timeout have distinct result states/messages;
- existing row cap remains in force and is not presented as an execution deadline;
- interruption is invoked in a thread-safe way documented by the SQLite/Qt contract;
- handler state cannot outlive the worker/database.

Stop conditions:

- the deployed Qt driver does not expose a safe interruption/progress seam;
- implementation requires sharing `QSqlDatabase` across threads;
- a long query cannot be deterministically terminated in teardown tests.

If blocked, record the driver/platform evidence and keep the finding open. Do not claim cancellation based only on
discarding a late result.

Feasibility outcome (2026-07-13): blocked at the planned stop condition.

- Qt's current QSQLITE implementation reports `QSqlDriver::CancelQuery` as unsupported and does not override the
  thread-safe `QSqlDriver::cancelQuery()` hook. Calling that public Qt API therefore cannot interrupt a running query.
- `QSqlDriver::handle()` can expose a `sqlite3*`, and SQLite documents `sqlite3_interrupt()` as safe from another thread
  while the connection is guaranteed not to close. SQLite also exposes a progress handler suitable for deadlines.
- Calling either native function still requires the exact SQLite ABI/library instance used by the loaded QSQLITE
  plugin. The current macOS Homebrew plugin links `/usr/lib/libsqlite3.dylib`, Linux packages use distribution Qt/SQLite,
  while official Windows Qt packages may compile bundled SQLite into `qsqlite.dll`. Linking a separate SQLite library
  and passing it the plugin-owned pointer is not a portable or supportable contract.
- Dynamically guessing or resolving symbols from the driver plugin is rejected because symbol visibility and library
  identity are not guaranteed across packaged platforms.

Decision outcome:

1. Selected: replace Qt SQL inside this isolated report subsystem with a directly owned SQLite C adapter and add one
   explicitly versioned SQLite dependency/package path. This gives the worker a stable connection handle, progress
   handler, interruption, deadlines, and deterministic tests, but requires a focused database-adapter migration and
   packaging verification on all three platforms.
2. Keep QSQLITE and accept stale-result suppression without real interruption. This avoids a dependency but leaves P1-2
   and the plan exit gate open; it must not be described as cancellation.
3. Build and ship a custom QSQLITE plugin with cancellation support. This depends on Qt SQL driver implementation details
   and duplicates Qt deployment responsibility, so it is not recommended.

Primary evidence: Qt 6 QSQLITE source (`QSQLiteDriver::hasFeature(CancelQuery) == false`), Qt's public
`QSqlDriver::handle()`/`cancelQuery()` contracts, SQLite's `sqlite3_interrupt()` and `sqlite3_progress_handler()`
contracts, local Qt 6.11.1 plugin linkage, and repository CI/package Qt installation paths.

### S3A — Own The SQLite Connection Directly — Complete

- `TherionSqlReportDatabase` now owns a native in-memory SQLite connection and prepared statements without QSQLITE,
  `QSqlDatabase`, Qt private APIs, or configure-time downloads.
- macOS/Linux use CMake's system SQLite target; Windows uses the Windows SDK `winsqlite3` API. Linux source/package
  builders install the development package explicitly, Debian shlibdeps discovers the runtime dependency, and AppImage
  staging includes `libsqlite3`.
- Existing statement allow-listing, transactional rollback, schema validation, read-only custom-query policy, row cap,
  null/integer/real/text/blob display conversion, worker affinity, and lifecycle behavior remain covered by focused
  tests.
- S3A deliberately adds no cancellation claim. Its purpose is to establish the stable owned handle required by S3B.

### S3B — Add Interruption And Deadline Policy — Complete

1. Add connection-lifetime synchronization that prevents close from racing `sqlite3_interrupt()`.
2. Install a per-operation progress handler with atomic cancellation and monotonic deadline state.
3. Distinguish user/supersession cancellation from deadline expiry in worker result codes and translated presentation.
4. Interrupt active work immediately when a newer request or session shutdown supersedes it.
5. Prove cancellation, timeout, recovery, and bounded teardown with deterministic recursive-query tests.

Implemented outcome:

- `TherionSqlReportExecutionControl` generation-keys operations, synchronizes native connection attach/detach against
  cross-thread `sqlite3_interrupt()`, and carries atomic cancellation and monotonic deadline state.
- The connection-owned progress handler enforces the ten-second production query deadline. A newer import/query and
  session shutdown interrupt active work immediately; cancellation and timeout use distinct worker result codes and
  translated messages.
- Recursive CTE tests cover deadline expiry, supersession cancellation, successful reuse after either outcome, and
  interrupted teardown within a fixed bound.

## S4 — Progress And Recovery — Complete

- Import progress may report statements/bytes processed at a bounded frequency.
- Query execution reports indeterminate busy state unless the SQLite policy provides a meaningful bounded metric.
- A cancelled/failed request shall leave the worker able to import/query again.
- Status and error strings are translated at the presentation boundary or through explicit translation contexts.
- No request logs SQL text or imported source content.

The tab already presents bounded busy state for import and indeterminate query execution. Import progress remains an
optional future UX enhancement; it is not required for the responsiveness exit gate. Recovery after timeout and
cancellation is now deterministic worker-test coverage.

## S5 — Extract Report Persistence And CSV IO — Complete

Only after S1-S4:

1. Inject the preset store/settings adapter instead of constructing/owning real `QSettings` in the widget.
2. Extract deterministic CSV serialization from path/dialog handling.
3. Route file writing through a focused exporter/file workflow; the tab selects export intent and presents errors.
4. Preserve CSV quoting, headers, row order, and existing user workflow.

Keep this as a separate commit chain from worker changes.

Implemented outcome:

- `MainWindow` composes a settings-backed preset-store adapter and CSV file exporter, then injects both into the tab.
  `TherionSqlReportTab` no longer constructs or owns `QSettings`.
- CSV quoting and UTF-8 serialization are deterministic in `TherionSqlReportCsvFileExporter`; the tab retains only the
  save dialog, export intent, and translated success/failure presentation.
- Focused tests cover settings-backed preset round-trip, injected tab fakes, CSV quoting/header/row ordering parity,
  file output, and real-import teardown after the worker connection has opened.

## Verification Matrix

- `TherionSqlReportDatabaseTest`: statement splitting, read-only policy, schema validation, row cap, malformed rollback.
- New worker QTest: thread affinity, large import, supersession, cancel, timeout, recovery, teardown.
- Tab/controller QTest: busy state, stale suppression, current-table behavior, translated errors.
- CSV parity test after S5.
- Cross-platform Debug CI, structure constraints, localization checker, `git diff --check`.

## Exit Gate

- Import and queries do not block the GUI event loop.
- A stale result cannot replace a newer file/query result.
- Cancellation interrupts active SQLite execution and tab teardown is bounded.
- Connection creation/use/destruction is worker-thread-affine.
- Preset and CSV behavior remains compatible and no real settings/file adapter is hidden in the widget.
